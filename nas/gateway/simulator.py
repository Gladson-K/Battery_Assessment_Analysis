#!/usr/bin/env python3
"""Synthetic telemetry publisher for dashboard and API testing."""

from __future__ import annotations

import argparse
import json
import math
import os
import random
import signal
import time
from datetime import datetime, timezone
from typing import Any

import paho.mqtt.client as mqtt
from paho.mqtt.enums import CallbackAPIVersion

PROFILES = {
    "bench_6v": {"voltage": 6.0, "current": 0.8, "temps": 8, "base": 32.0},
    "ev_pack_48v": {"voltage": 51.2, "current": 40.0, "temps": 10, "base": 38.0},
    "server_ups_24v": {"voltage": 25.6, "current": 12.0, "temps": 5, "base": 34.0},
    "solar_cabinet_12v": {"voltage": 12.8, "current": 6.0, "temps": 4, "base": 36.0},
    "cold_chain_12v": {"voltage": 12.4, "current": 4.0, "temps": 2, "base": 6.0},
}


def now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


class Simulator:
    def __init__(self, device_id: str, profile: str, interval: float) -> None:
        self.device_id = device_id
        self.profile = profile
        self.spec = PROFILES.get(profile, PROFILES["bench_6v"])
        self.interval = interval
        self.seq = 0
        self.started = time.monotonic()
        self.client = mqtt.Client(callback_api_version=CallbackAPIVersion.VERSION2, client_id=f"sim-{device_id}")
        username = os.getenv("MQTT_USERNAME", "")
        if username:
            self.client.username_pw_set(username, os.getenv("MQTT_PASSWORD", ""))
        self.client.on_connect = self.on_connect
        self.client.on_message = self.on_message

    def on_connect(self, client: mqtt.Client, userdata: Any, flags: Any, reason_code: Any, properties: Any = None) -> None:
        client.subscribe(f"battery/{self.device_id}/cmd", qos=1)
        client.publish(
            f"battery/{self.device_id}/status",
            json.dumps(
                {
                    "device_id": self.device_id,
                    "profile": self.profile,
                    "location": "simulator",
                    "online": True,
                    "last_seen": now(),
                }
            ),
            qos=1,
            retain=True,
        )

    def on_message(self, client: mqtt.Client, userdata: Any, message: mqtt.MQTTMessage) -> None:
        try:
            payload = json.loads(message.payload)
            command = str(payload.get("cmd", "")).upper()
        except Exception:
            command = message.payload.decode("utf-8", errors="replace").strip().upper()
        response = {"ok": command in {"PING", "HEARTBEAT", "STATUS", "RESET", "CAL", "SELFTEST", "HELP"}}
        if response["ok"]:
            response["cmd"] = command
        else:
            response["error"] = "unknown_command"
        client.publish(f"battery/{self.device_id}/ack", json.dumps(response), qos=1)

    def sample(self) -> dict[str, Any]:
        t = time.monotonic() - self.started
        base = self.spec["base"]
        wave = 8.0 * math.sin(t / 45.0) + 3.0 * math.sin(t / 7.0)
        hotspot = max(0.0, 20.0 * math.sin(t / 120.0))
        temperatures = [round(base + wave + hotspot + random.uniform(-0.8, 0.8), 2) for _ in range(self.spec["temps"])]
        tmax = max(temperatures)
        if tmax >= 85:
            state = "TRIPPED"
        elif tmax >= 75:
            state = "CRITICAL"
        elif tmax >= 60:
            state = "WARNING"
        else:
            state = "NORMAL"
        risk = min(100.0, max(0.0, (tmax - 25.0) * 1.7 + random.uniform(-1.0, 1.0)))
        voltage = self.spec["voltage"] + random.uniform(-0.15, 0.15)
        current = max(0.0, self.spec["current"] + random.uniform(-1.0, 1.0))
        self.seq = (self.seq + 1) & 0xFFFF
        return {
            "seq": self.seq,
            "ms": int(t * 1000),
            "state": state,
            "risk": round(risk, 2),
            "rule": round(risk, 2),
            "ml": {
                "p": round(min(1.0, max(0.0, (tmax - 30.0) / 60.0)), 4),
                "label": 1 if tmax > 70 else 0,
                "valid": 1,
                "lowConf": 0,
            },
            "v": round(voltage, 3),
            "i": round(current, 3),
            "p": round(voltage * current, 3),
            "rain": round(random.uniform(0.0, 100.0), 1) if self.profile in {"bench_6v", "solar_cabinet_12v"} else 0.0,
            "rainDry": 1,
            "hum": round(random.uniform(30.0, 75.0), 1),
            "t": temperatures,
            "tmin": min(temperatures),
            "tmax": tmax,
            "spread": round(max(temperatures) - min(temperatures), 2),
            "dt": round(random.uniform(-0.1, 0.2), 3),
            "faults": "0x00000000",
            "latched": "0x00000000",
            "reason": state.lower(),
            "relay": 1 if state not in {"CRITICAL", "TRIPPED"} else 0,
            "buzzer": 1 if state in {"WARNING", "CRITICAL", "TRIPPED"} else 0,
            "ver": "1.0.0",
            "device_id": self.device_id,
            "profile": self.profile,
            "location": "simulator",
            "received_at": now(),
        }

    def run(self) -> None:
        host = os.getenv("MQTT_HOST", "localhost")
        port = int(os.getenv("MQTT_PORT", "1883"))
        self.client.connect(host, port, keepalive=30)
        self.client.loop_start()
        try:
            while True:
                self.client.publish(f"battery/{self.device_id}/telemetry", json.dumps(self.sample()), qos=1)
                time.sleep(self.interval)
        except KeyboardInterrupt:
            pass
        finally:
            self.client.publish(
                f"battery/{self.device_id}/status",
                json.dumps(
                    {
                        "device_id": self.device_id,
                        "profile": self.profile,
                        "location": "simulator",
                        "online": False,
                        "last_seen": now(),
                    }
                ),
                qos=1,
                retain=True,
            )
            self.client.loop_stop()
            self.client.disconnect()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--device", default="bench-01")
    parser.add_argument("--profile", default="bench_6v", choices=sorted(PROFILES))
    parser.add_argument("--interval", type=float, default=1.0)
    args = parser.parse_args()
    sim = Simulator(args.device, args.profile, args.interval)
    signal.signal(signal.SIGTERM, lambda *_: raise_exit())
    sim.run()
    return 0


def raise_exit() -> None:
    raise KeyboardInterrupt


if __name__ == "__main__":
    raise SystemExit(main())
