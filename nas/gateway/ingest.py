#!/usr/bin/env python3
"""MQTT to InfluxDB ingester with a local SQLite store-and-forward buffer."""

from __future__ import annotations

import json
import logging
import os
import sqlite3
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion
from influxdb_client import InfluxDBClient, Point, WritePrecision
from influxdb_client.client.write_api import SYNCHRONOUS

LOG = logging.getLogger("ingest")


def utc_now() -> datetime:
    return datetime.now(timezone.utc)


def parse_time(value: Any) -> datetime:
    if not value:
        return utc_now()
    try:
        return datetime.fromisoformat(str(value).replace("Z", "+00:00"))
    except ValueError:
        return utc_now()


def as_float(value: Any) -> float | None:
    if value is None:
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def as_int(value: Any) -> int | None:
    if value is None:
        return None
    try:
        return int(value)
    except (TypeError, ValueError):
        return None


class Buffer:
    def __init__(self, path: str) -> None:
        self.path = path
        Path(path).parent.mkdir(parents=True, exist_ok=True)
        self.lock = threading.Lock()
        self.connection = sqlite3.connect(path, check_same_thread=False)
        self.connection.execute(
            "CREATE TABLE IF NOT EXISTS pending ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "measurement TEXT NOT NULL,"
            "payload TEXT NOT NULL,"
            "created_at TEXT NOT NULL)"
        )
        self.connection.commit()

    def put(self, measurement: str, payload: dict[str, Any]) -> None:
        with self.lock:
            self.connection.execute(
                "INSERT INTO pending (measurement, payload, created_at) VALUES (?, ?, ?)",
                (measurement, json.dumps(payload, separators=(",", ":")), utc_now().isoformat()),
            )
            self.connection.commit()

    def take(self, limit: int = 250) -> list[tuple[int, str, dict[str, Any]]]:
        with self.lock:
            rows = self.connection.execute(
                "SELECT id, measurement, payload FROM pending ORDER BY id LIMIT ?", (limit,)
            ).fetchall()
        return [(row[0], row[1], json.loads(row[2])) for row in rows]

    def delete(self, ids: list[int]) -> None:
        if not ids:
            return
        with self.lock:
            placeholders = ",".join("?" for _ in ids)
            self.connection.execute(f"DELETE FROM pending WHERE id IN ({placeholders})", ids)
            self.connection.commit()

    def count(self) -> int:
        with self.lock:
            row = self.connection.execute("SELECT COUNT(*) FROM pending").fetchone()
        return int(row[0]) if row else 0

    def close(self) -> None:
        with self.lock:
            self.connection.close()


class Ingester:
    def __init__(self) -> None:
        self.mqtt_host = os.getenv("MQTT_HOST", "mosquitto")
        self.mqtt_port = int(os.getenv("MQTT_PORT", "1883"))
        self.mqtt_username = os.getenv("MQTT_USERNAME", "")
        self.mqtt_password = os.getenv("MQTT_PASSWORD", "")
        self.influx_url = os.getenv("INFLUX_URL", "http://influxdb:8086")
        self.influx_token = os.getenv("INFLUX_TOKEN", "")
        self.influx_org = os.getenv("INFLUX_ORG", "battery")
        self.influx_bucket = os.getenv("INFLUX_BUCKET", "battery")
        self.buffer = Buffer(os.getenv("BUFFER_DB", "/data/buffer.db"))
        self.client = InfluxDBClient(url=self.influx_url, token=self.influx_token, org=self.influx_org)
        self.write_api = self.client.write_api(write_options=SYNCHRONOUS)
        self.stop_event = threading.Event()
        self.pending: list[tuple[str, dict[str, Any]]] = []
        self.pending_lock = threading.Lock()
        self.last_flush = time.monotonic()
        self.mqtt = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2, client_id="battery-ingest")
        if self.mqtt_username:
            self.mqtt.username_pw_set(self.mqtt_username, self.mqtt_password)
        self.mqtt.on_connect = self._on_connect
        self.mqtt.on_message = self._on_message
        self.mqtt.on_disconnect = self._on_disconnect
        self.mqtt.reconnect_delay_set(min_delay=1, max_delay=30)

    def start(self) -> None:
        if not self.influx_token:
            raise RuntimeError("INFLUX_TOKEN is required")
        self.mqtt.connect_async(self.mqtt_host, self.mqtt_port, keepalive=30)
        self.mqtt.loop_start()
        flush_thread = threading.Thread(target=self._flush_loop, daemon=True, name="flush")
        replay_thread = threading.Thread(target=self._replay_loop, daemon=True, name="replay")
        flush_thread.start()
        replay_thread.start()
        try:
            while not self.stop_event.is_set():
                time.sleep(0.5)
        finally:
            self.stop()

    def stop(self) -> None:
        self.stop_event.set()
        self._flush(force=True)
        try:
            self.mqtt.loop_stop()
            self.mqtt.disconnect()
            self.client.close()
        except Exception:  # pragma: no cover
            pass
        self.buffer.close()

    def _on_connect(self, client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any = None) -> None:
        LOG.info("MQTT connected: %s", reason_code)
        client.subscribe("battery/+/telemetry", qos=1)
        client.subscribe("battery/+/status", qos=1)

    def _on_disconnect(self, client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any = None) -> None:
        LOG.warning("MQTT disconnected: %s", reason_code)

    def _on_message(self, client: mqtt.Client, userdata: Any, message: mqtt.MQTTMessage) -> None:
        try:
            payload = json.loads(message.payload)
            if not isinstance(payload, dict):
                return
            if message.topic.endswith("/telemetry"):
                with self.pending_lock:
                    self.pending.append(("battery_telemetry", payload))
            elif message.topic.endswith("/status"):
                self._write_points([self._status_point(payload)])
        except Exception:
            LOG.exception("failed to ingest %s", message.topic)

    def _telemetry_point(self, payload: dict[str, Any]) -> Point:
        point = Point("battery_telemetry")
        for tag in ("device_id", "profile", "location", "state", "ver"):
            value = payload.get(tag)
            if value not in (None, ""):
                point.tag(tag, str(value))

        fields: dict[str, Any] = {
            "sequence": as_int(payload.get("seq")),
            "device_ms": as_int(payload.get("ms")),
            "risk": as_float(payload.get("risk")),
            "rule": as_float(payload.get("rule")),
            "voltage_v": as_float(payload.get("v")),
            "current_a": as_float(payload.get("i")),
            "power_w": as_float(payload.get("p")),
            "rain_pct": as_float(payload.get("rain")),
            "rain_dry": as_int(payload.get("rainDry")),
            "humidity_pct": as_float(payload.get("hum")),
            "t_min": as_float(payload.get("tmin")),
            "t_max": as_float(payload.get("tmax")),
            "t_spread": as_float(payload.get("spread")),
            "dt_c_per_s": as_float(payload.get("dt")),
            "relay_allowed": as_int(payload.get("relay")),
            "buzzer_on": as_int(payload.get("buzzer")),
        }
        ml = payload.get("ml") or {}
        if isinstance(ml, dict):
            fields.update(
                {
                    "ml_p_unsafe": as_float(ml.get("p")),
                    "ml_label": as_int(ml.get("label")),
                    "ml_valid": as_int(ml.get("valid")),
                    "ml_low_confidence": as_int(ml.get("lowConf")),
                }
            )
        for key, value in list(fields.items()):
            if value is None:
                fields.pop(key)
        for key, value in fields.items():
            point.field(key, value)
        if payload.get("faults"):
            point.field("faults_active", str(payload["faults"]))
        if payload.get("latched"):
            point.field("faults_latched", str(payload["latched"]))
        if payload.get("reason"):
            point.field("reason", str(payload["reason"]))
        temperatures = payload.get("t") or []
        if isinstance(temperatures, list):
            for index, value in enumerate(temperatures):
                numeric = as_float(value)
                if numeric is not None:
                    point.field(f"t{index}", numeric)
        point.time(parse_time(payload.get("received_at")), WritePrecision.MS)
        return point

    def _status_point(self, payload: dict[str, Any]) -> Point:
        point = Point("battery_status")
        for tag in ("device_id", "profile", "location"):
            value = payload.get(tag)
            if value not in (None, ""):
                point.tag(tag, str(value))
        point.field("online", 1 if payload.get("online") else 0)
        if payload.get("last_seen"):
            point.field("last_seen", str(payload["last_seen"]))
        if payload.get("serial_port"):
            point.field("serial_port", str(payload["serial_port"]))
        point.time(parse_time(payload.get("received_at")), WritePrecision.MS)
        return point

    def _write_points(self, points: list[Point]) -> bool:
        if not points:
            return True
        try:
            self.write_api.write(bucket=self.influx_bucket, org=self.influx_org, record=points)
            return True
        except Exception as exc:
            LOG.warning("InfluxDB write failed: %s", exc)
            return False

    def _flush_loop(self) -> None:
        while not self.stop_event.is_set():
            self.stop_event.wait(1.0)
            self._flush()

    def _flush(self, force: bool = False) -> None:
        with self.pending_lock:
            if not self.pending:
                return
            if not force and len(self.pending) < int(os.getenv("BATCH_SIZE", "50")) and time.monotonic() - self.last_flush < float(os.getenv("FLUSH_INTERVAL_S", "2")):
                return
            batch = self.pending
            self.pending = []
            self.last_flush = time.monotonic()
        points: list[Point] = []
        for measurement, payload in batch:
            try:
                points.append(self._telemetry_point(payload) if measurement == "battery_telemetry" else self._status_point(payload))
            except Exception:
                LOG.exception("failed to build point")
        if self._write_points(points):
            LOG.debug("wrote %d points", len(points))
        else:
            for measurement, payload in batch:
                self.buffer.put(measurement, payload)
            LOG.warning("buffered %d points; pending=%d", len(batch), self.buffer.count())

    def _replay_loop(self) -> None:
        while not self.stop_event.is_set():
            self.stop_event.wait(10.0)
            if self.buffer.count() == 0:
                continue
            rows = self.buffer.take(int(os.getenv("REPLAY_BATCH", "250")))
            points: list[Point] = []
            for _, measurement, payload in rows:
                try:
                    points.append(self._telemetry_point(payload) if measurement == "battery_telemetry" else self._status_point(payload))
                except Exception:
                    LOG.exception("failed to rebuild buffered point")
            if self._write_points(points):
                self.buffer.delete([row[0] for row in rows])
                LOG.info("replayed %d buffered points", len(rows))
            else:
                break


def main() -> int:
    logging.basicConfig(
        level=os.getenv("LOG_LEVEL", "INFO").upper(),
        format="%(asctime)s %(levelname)s %(name)s %(message)s",
    )
    ingester = Ingester()
    try:
        ingester.start()
    except KeyboardInterrupt:
        ingester.stop_event.set()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
