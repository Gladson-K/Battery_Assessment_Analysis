#!/usr/bin/env python3
"""Serial-to-MQTT gateway for one or more STM32 battery controllers."""

from __future__ import annotations

import argparse
import json
import logging
import os
import signal
import threading
import time
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion
import serial
import yaml

from protocol import Frame, FrameParser, encode_frame

LOG = logging.getLogger("serial-gateway")

ALLOWED_COMMANDS = {"PING", "HEARTBEAT", "STATUS", "RESET", "CAL", "SELFTEST", "HELP"}


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


@dataclass
class Device:
    device_id: str
    port: str
    baud: int
    profile: str
    location: str
    heartbeat_interval_s: float = 5.0
    enabled: bool = True
    serial: serial.Serial | None = None
    write_lock: threading.Lock = field(default_factory=threading.Lock)

    @property
    def telemetry_topic(self) -> str:
        return f"battery/{self.device_id}/telemetry"

    @property
    def status_topic(self) -> str:
        return f"battery/{self.device_id}/status"

    @property
    def ack_topic(self) -> str:
        return f"battery/{self.device_id}/ack"

    def open_serial(self) -> None:
        self.serial = serial.Serial(self.port, self.baud, timeout=1.0, write_timeout=2.0)
        LOG.info("opened %s at %d baud for %s", self.port, self.baud, self.device_id)

    def close_serial(self) -> None:
        if self.serial is not None:
            try:
                self.serial.close()
            except Exception:  # pragma: no cover - best effort
                pass
            self.serial = None

    def write_command(self, command: str) -> bool:
        if self.serial is None:
            return False
        frame = encode_frame(command)
        with self.write_lock:
            try:
                self.serial.write(frame)
                self.serial.flush()
                return True
            except serial.SerialException as exc:
                LOG.warning("serial write failed for %s: %s", self.device_id, exc)
                return False


def load_config(path: str) -> tuple[dict[str, Any], list[Device]]:
    mqtt_cfg: dict[str, Any] = {
        "host": os.getenv("MQTT_HOST", "mosquitto"),
        "port": int(os.getenv("MQTT_PORT", "1883")),
        "username": os.getenv("MQTT_USERNAME", ""),
        "password": os.getenv("MQTT_PASSWORD", ""),
        "base_topic": "battery",
    }
    devices: list[Device] = []
    config_path = Path(path)
    if config_path.exists():
        raw = yaml.safe_load(config_path.read_text()) or {}
        mqtt_cfg.update(raw.get("mqtt") or {})
        for item in raw.get("devices") or []:
            devices.append(
                Device(
                    device_id=str(item["id"]),
                    port=str(item.get("port", "/dev/ttyUSB0")),
                    baud=int(item.get("baud", 115200)),
                    profile=str(item.get("profile", "bench_6v")),
                    location=str(item.get("location", "")),
                    heartbeat_interval_s=float(item.get("heartbeat_interval_s", 5)),
                    enabled=bool(item.get("enabled", True)),
                )
            )
    else:
        devices.append(
            Device(
                device_id=os.getenv("DEVICE_ID", "bench-01"),
                port=os.getenv("SERIAL_PORT", "/dev/ttyUSB0"),
                baud=int(os.getenv("SERIAL_BAUD", "115200")),
                profile=os.getenv("PROFILE", "bench_6v"),
                location=os.getenv("LOCATION", "lab"),
                heartbeat_interval_s=float(os.getenv("HEARTBEAT_INTERVAL_S", "5")),
            )
        )

    mqtt_cfg["host"] = os.getenv("MQTT_HOST", mqtt_cfg.get("host", "mosquitto"))
    mqtt_cfg["port"] = int(os.getenv("MQTT_PORT", str(mqtt_cfg.get("port", 1883))))
    mqtt_cfg["username"] = os.getenv("MQTT_USERNAME", mqtt_cfg.get("username", ""))
    mqtt_cfg["password"] = os.getenv("MQTT_PASSWORD", mqtt_cfg.get("password", ""))
    mqtt_cfg["base_topic"] = os.getenv("MQTT_BASE_TOPIC", mqtt_cfg.get("base_topic", "battery"))
    return mqtt_cfg, devices


class Gateway:
    def __init__(self, mqtt_cfg: dict[str, Any], devices: list[Device]) -> None:
        self.mqtt_cfg = mqtt_cfg
        self.devices = {d.device_id: d for d in devices}
        self.stop_event = threading.Event()
        self.client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2, client_id="battery-serial-gateway")
        if mqtt_cfg.get("username"):
            self.client.username_pw_set(mqtt_cfg["username"], mqtt_cfg.get("password", ""))
        self.client.on_connect = self._on_connect
        self.client.on_message = self._on_message
        self.client.on_disconnect = self._on_disconnect
        self.client.reconnect_delay_set(min_delay=1, max_delay=30)

    def start(self) -> None:
        self.client.connect_async(self.mqtt_cfg["host"], self.mqtt_cfg["port"], keepalive=30)
        self.client.loop_start()
        threads = []
        for device in self.devices.values():
            if not device.enabled:
                continue
            thread = threading.Thread(target=self._device_loop, args=(device,), daemon=True, name=device.device_id)
            thread.start()
            threads.append(thread)
        try:
            while not self.stop_event.is_set():
                time.sleep(0.5)
        finally:
            self.stop()
            for thread in threads:
                thread.join(timeout=3)

    def stop(self) -> None:
        self.stop_event.set()
        for device in self.devices.values():
            device.close_serial()
        try:
            self.client.loop_stop()
            self.client.disconnect()
        except Exception:  # pragma: no cover - shutdown best effort
            pass

    def _publish(self, topic: str, payload: dict[str, Any], retain: bool = False) -> None:
        self.client.publish(topic, json.dumps(payload, separators=(",", ":")), qos=1, retain=retain)

    def _on_connect(self, client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any = None) -> None:
        LOG.info("MQTT connected: %s", reason_code)
        for device in self.devices.values():
            client.subscribe(f"battery/{device.device_id}/cmd", qos=1)

    def _on_disconnect(self, client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any = None) -> None:
        LOG.warning("MQTT disconnected: %s", reason_code)

    def _on_message(self, client: mqtt.Client, userdata: Any, message: mqtt.MQTTMessage) -> None:
        try:
            parts = message.topic.split("/")
            device = self.devices.get(parts[1]) if len(parts) >= 3 else None
            if device is None:
                return
            command = self._decode_command(message.payload)
            if command is None:
                LOG.warning("rejected command for %s: %r", device.device_id, message.payload[:80])
                self._publish(device.ack_topic, {"ok": False, "error": "invalid_command"})
                return
            LOG.info("command for %s: %s", device.device_id, command)
            if not device.write_command(command):
                self._publish(device.ack_topic, {"ok": False, "error": "serial_unavailable", "cmd": command})
        except Exception:
            LOG.exception("command handling failed")

    @staticmethod
    def _decode_command(payload: bytes) -> str | None:
        text = payload.decode("utf-8", errors="replace").strip()
        if not text:
            return None
        if text.startswith("{"):
            try:
                data = json.loads(text)
            except json.JSONDecodeError:
                return None
            text = str(data.get("cmd", "")).strip()
        command = text.upper()
        return command if command in ALLOWED_COMMANDS else None

    def _device_loop(self, device: Device) -> None:
        parser = FrameParser()
        last_heartbeat = 0.0
        last_seen = 0.0
        online: bool | None = None
        while not self.stop_event.is_set():
            try:
                if device.serial is None:
                    device.open_serial()
                    parser.reset()
                now = time.monotonic()
                if now - last_heartbeat >= device.heartbeat_interval_s:
                    device.write_command("HEARTBEAT")
                    last_heartbeat = now
                data = device.serial.read(device.serial.in_waiting or 1)
                if data:
                    for frame in parser.feed(data):
                        if self._handle_frame(device, frame):
                            last_seen = time.monotonic()
                            if online is not True:
                                online = True
                                self._publish(
                                    device.status_topic,
                                    {
                                        "device_id": device.device_id,
                                        "profile": device.profile,
                                        "location": device.location,
                                        "online": True,
                                        "last_seen": utc_now(),
                                        "serial_port": device.port,
                                    },
                                    retain=True,
                                )
                if online is not False and last_seen and time.monotonic() - last_seen > 30:
                    online = False
                    self._publish(
                        device.status_topic,
                        {
                            "device_id": device.device_id,
                            "profile": device.profile,
                            "location": device.location,
                            "online": False,
                            "last_seen": utc_now(),
                            "serial_port": device.port,
                        },
                        retain=True,
                    )
            except serial.SerialException as exc:
                LOG.warning("serial error for %s: %s", device.device_id, exc)
                device.close_serial()
                if online is not False:
                    online = False
                    self._publish(
                        device.status_topic,
                        {
                            "device_id": device.device_id,
                            "profile": device.profile,
                            "location": device.location,
                            "online": False,
                            "last_seen": utc_now(),
                            "serial_port": device.port,
                        },
                        retain=True,
                    )
                self.stop_event.wait(5)
            except Exception:
                LOG.exception("device loop failed for %s", device.device_id)
                device.close_serial()
                self.stop_event.wait(5)
        device.close_serial()

    def _handle_frame(self, device: Device, frame: Frame) -> bool:
        if not frame.crc_ok or frame.data is None:
            LOG.debug("dropped frame from %s: %s", device.device_id, frame.error)
            return False
        data = dict(frame.data)
        data.setdefault("device_id", device.device_id)
        data.setdefault("profile", device.profile)
        data.setdefault("location", device.location)
        data["received_at"] = utc_now()
        if "seq" in data and "state" in data:
            self._publish(device.telemetry_topic, data)
        elif "ok" in data:
            self._publish(device.ack_topic, data)
        else:
            self._publish(device.ack_topic, data)
        return True


def main() -> int:
    logging.basicConfig(
        level=os.getenv("LOG_LEVEL", "INFO").upper(),
        format="%(asctime)s %(levelname)s %(name)s %(message)s",
    )
    parser = argparse.ArgumentParser()
    parser.add_argument("--config", default=os.getenv("DEVICES_CONFIG", "/app/devices.yml"))
    args = parser.parse_args()
    mqtt_cfg, devices = load_config(args.config)
    if not devices:
        LOG.error("no devices configured")
        return 2
    LOG.info(
        "starting gateway for %d device(s) via MQTT %s:%s",
        len(devices),
        mqtt_cfg["host"],
        mqtt_cfg["port"],
    )
    gateway = Gateway(mqtt_cfg, devices)

    def handle_signal(signum: int, _frame: Any) -> None:
        LOG.info("signal %s received", signum)
        gateway.stop_event.set()

    signal.signal(signal.SIGTERM, handle_signal)
    signal.signal(signal.SIGINT, handle_signal)
    gateway.start()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
