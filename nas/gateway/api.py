#!/usr/bin/env python3
"""Read/command API for the battery telemetry platform."""

from __future__ import annotations

import os
import re
from datetime import datetime, timezone
from typing import Any

import paho.mqtt.publish as mqtt_publish
from fastapi import Depends, FastAPI, Header, HTTPException, Query
from influxdb_client import InfluxDBClient
from pydantic import BaseModel, Field

INFLUX_URL = os.getenv("INFLUX_URL", "http://influxdb:8086")
INFLUX_TOKEN = os.getenv("INFLUX_TOKEN", "")
INFLUX_ORG = os.getenv("INFLUX_ORG", "battery")
INFLUX_BUCKET = os.getenv("INFLUX_BUCKET", "battery")
MQTT_HOST = os.getenv("MQTT_HOST", "mosquitto")
MQTT_PORT = int(os.getenv("MQTT_PORT", "1883"))
MQTT_USERNAME = os.getenv("MQTT_USERNAME", "")
MQTT_PASSWORD = os.getenv("MQTT_PASSWORD", "")
API_KEY = os.getenv("API_KEY", "")

ALLOWED_COMMANDS = {"PING", "HEARTBEAT", "STATUS", "RESET", "CAL", "SELFTEST", "HELP"}
SAFE_ID = re.compile(r"^[A-Za-z0-9_.:-]{1,64}$")

app = FastAPI(title="Battery Safety API", version="1.0.0")
client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
query_api = client.query_api()


def require_key(x_api_key: str | None = Header(default=None)) -> None:
    if API_KEY and x_api_key != API_KEY:
        raise HTTPException(status_code=401, detail="invalid API key")


def safe_device(device_id: str) -> str:
    if not SAFE_ID.fullmatch(device_id):
        raise HTTPException(status_code=400, detail="invalid device id")
    return device_id


def iso(value: Any) -> str | None:
    if value is None:
        return None
    if isinstance(value, datetime):
        return value.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")
    return str(value)


def query(flux: str) -> list[dict[str, Any]]:
    result: list[dict[str, Any]] = []
    try:
        tables = query_api.query(flux)
    except Exception as exc:  # pragma: no cover - depends on live InfluxDB
        raise HTTPException(status_code=502, detail=f"influx query failed: {exc}") from exc
    for table in tables:
        for record in table.records:
            values = dict(record.values)
            values["_time"] = iso(values.get("_time"))
            result.append(values)
    return result


@app.get("/health")
def health() -> dict[str, Any]:
    try:
        influx_ok = client.ping()
    except Exception:
        influx_ok = False
    return {
        "ok": bool(influx_ok),
        "influx": bool(influx_ok),
        "mqtt_host": MQTT_HOST,
        "bucket": INFLUX_BUCKET,
    }


@app.get("/devices", dependencies=[Depends(require_key)])
def devices() -> list[dict[str, Any]]:
    flux = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: -7d)
  |> filter(fn: (r) => r._measurement == "battery_status" and r._field == "online")
  |> last()
'''
    rows = query(flux)
    latest: dict[str, dict[str, Any]] = {}
    for row in rows:
        device = row.get("device_id")
        if not device:
            continue
        latest[device] = {
            "device_id": device,
            "profile": row.get("profile"),
            "location": row.get("location"),
            "online": bool(row.get("_value")),
            "last_seen": row.get("_time"),
        }
    return sorted(latest.values(), key=lambda item: item["device_id"])


@app.get("/devices/{device_id}/latest", dependencies=[Depends(require_key)])
def latest(device_id: str) -> dict[str, Any]:
    device_id = safe_device(device_id)
    flux = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: -24h)
  |> filter(fn: (r) => r._measurement == "battery_telemetry" and r.device_id == "{device_id}")
  |> last()
'''
    rows = query(flux)
    if not rows:
        raise HTTPException(status_code=404, detail="no telemetry")
    latest_time = max((row.get("_time") or "" for row in rows), default="")
    values: dict[str, Any] = {}
    tags: dict[str, Any] = {}
    for row in rows:
        field = row.get("_field")
        if field:
            values[field] = row.get("_value")
        for tag in ("profile", "location", "state", "ver"):
            if row.get(tag) is not None:
                tags[tag] = row.get(tag)
    return {"device_id": device_id, "time": latest_time, "tags": tags, "values": values}


@app.get("/devices/{device_id}/history", dependencies=[Depends(require_key)])
def history(
    device_id: str,
    field: str = Query(default="risk", pattern=r"^[A-Za-z0-9_]+$"),
    minutes: int = Query(default=60, ge=1, le=10080),
    window: str = Query(default="1m", pattern=r"^[0-9]+[smhd]$"),
) -> list[dict[str, Any]]:
    device_id = safe_device(device_id)
    flux = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: -{minutes}m)
  |> filter(fn: (r) => r._measurement == "battery_telemetry" and r.device_id == "{device_id}" and r._field == "{field}")
  |> aggregateWindow(every: {window}, fn: mean, createEmpty: false)
  |> keep(columns: ["_time", "_value"])
'''
    return query(flux)


@app.get("/alerts", dependencies=[Depends(require_key)])
def alerts(minutes: int = Query(default=1440, ge=1, le=10080)) -> list[dict[str, Any]]:
    flux = f'''
from(bucket: "{INFLUX_BUCKET}")
  |> range(start: -{minutes}m)
  |> filter(fn: (r) => r._measurement == "battery_telemetry" and r._field == "risk")
  |> filter(fn: (r) => r._value >= 70.0)
  |> sort(columns: ["_time"], desc: true)
  |> limit(n: 200)
'''
    return query(flux)


class CommandRequest(BaseModel):
    cmd: str = Field(..., description="PING, HEARTBEAT, STATUS, RESET, CAL, SELFTEST or HELP")


@app.post("/devices/{device_id}/command", dependencies=[Depends(require_key)])
def command(device_id: str, request: CommandRequest) -> dict[str, Any]:
    device_id = safe_device(device_id)
    cmd = request.cmd.strip().upper()
    if cmd not in ALLOWED_COMMANDS:
        raise HTTPException(status_code=400, detail=f"unsupported command: {cmd}")
    auth = {"username": MQTT_USERNAME, "password": MQTT_PASSWORD} if MQTT_USERNAME else None
    try:
        mqtt_publish.single(
            f"battery/{device_id}/cmd",
            payload=f'{{"cmd":"{cmd}"}}',
            qos=1,
            hostname=MQTT_HOST,
            port=MQTT_PORT,
            auth=auth,
        )
    except Exception as exc:  # pragma: no cover - depends on live broker
        raise HTTPException(status_code=502, detail=f"mqtt publish failed: {exc}") from exc
    return {"ok": True, "device_id": device_id, "cmd": cmd}
