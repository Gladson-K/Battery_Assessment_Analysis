# API and Telemetry Reference

## REST API

Base URL: `http://<nas>:8088`

Authentication: if `API_KEY` is set, send `X-API-Key: <value>` on every endpoint
except `/health` and `/docs`. If `API_KEY` is empty, authentication is disabled;
use this only on a trusted network.

### `GET /health`

```json
{"ok": true, "influx": true, "mqtt_host": "mosquitto", "bucket": "battery"}
```

`ok`/`influx` are true only when InfluxDB responds.

### `GET /devices`

Returns the latest status per device from the `battery_status` measurement.

```json
[
  {
    "device_id": "bench-01",
    "profile": "bench_6v",
    "location": "lab",
    "online": true,
    "last_seen": "2026-11-05T16:12:34.931000Z"
  }
]
```

### `GET /devices/{device_id}/latest`

Returns the most recent telemetry point in the last 24 hours. Values are keyed by
Influx field name.

```json
{
  "device_id": "bench-01",
  "time": "2026-11-05T16:12:34.931000Z",
  "tags": {"profile": "bench_6v", "location": "lab", "state": "NORMAL", "ver": "1.0.0"},
  "values": {
    "risk": 46.38,
    "voltage_v": 5.946,
    "current_a": 0.523,
    "t_max": 52.54,
    "faults_active": "0x00000000",
    "ml_p_unsafe": 0.3757
  }
}
```

Returns `404` if no telemetry exists. Device IDs must match
`[A-Za-z0-9_.:-]{1,64}`.

### `GET /devices/{device_id}/history`

Query parameters:

| Parameter | Default | Range | Meaning |
| --- | --- | --- | --- |
| `field` | `risk` | `^[A-Za-z0-9_]+$` | Influx field to aggregate |
| `minutes` | `60` | 1-10080 | Lookback window |
| `window` | `1m` | `<n><s/m/h/d>` | Aggregation window |

```bash
curl -H "X-API-Key: $API_KEY" \
  "http://nas:8088/devices/bench-01/history?field=risk&minutes=60&window=1m"
```

Returns a list of `{"_time": ..., "_value": ...}` points.

### `GET /alerts`

Returns up to 200 telemetry records where `risk >= 70` in the requested window.

| Parameter | Default | Range |
| --- | --- | --- |
| `minutes` | `1440` | 1-10080 |

### `POST /devices/{device_id}/command`

Publishes a command to `battery/<id>/cmd` with QoS 1.

```json
{"cmd": "STATUS"}
```

Allowed commands: `PING`, `HEARTBEAT`, `STATUS`, `RESET`, `CAL`, `SELFTEST`,
`HELP`.

Response:

```json
{"ok": true, "device_id": "bench-01", "cmd": "STATUS"}
```

Errors:

| Status | Meaning |
| --- | --- |
| `400` | Invalid device id, missing field or unsupported command |
| `401` | Invalid/missing API key |
| `404` | No telemetry for the device |
| `502` | Influx query or MQTT publish failed |

## MQTT topics

| Topic | Direction | Retained | Payload |
| --- | --- | --- | --- |
| `battery/<id>/telemetry` | controller/gateway -> NAS | no | Firmware JSON + device metadata |
| `battery/<id>/status` | gateway -> NAS | yes | Online/last-seen/profile/location |
| `battery/<id>/cmd` | API/user -> gateway | no | `{"cmd":"STATUS"}` or plain `STATUS` |
| `battery/<id>/ack` | controller -> NAS | no | Command response JSON |

The gateway uses `HEARTBEAT` every 5 s. `CAL` must be run with the load
disconnected. `RESET` reboots the controller.

## Serial frame

```text
$<json>*<4 hex CRC16-CCITT-FALSE>\n
```

- CRC polynomial: `0x1021`, init `0xFFFF`.
- Test vector: `"123456789"` -> `0x29B1`.
- CRC covers only the payload between `$` and `*`.
- The parser tolerates arbitrary UART chunk boundaries and rejects bad CRC,
  invalid UTF-8, non-JSON and non-object payloads.

## Firmware telemetry JSON

```json
{
  "seq": 87,
  "ms": 86091,
  "state": "NORMAL",
  "risk": 46.38,
  "rule": 46.38,
  "ml": {"p": 0.3757, "label": 0, "valid": 1, "lowConf": 0},
  "v": 5.946,
  "i": 0.523,
  "p": 3.108,
  "rain": 27.9,
  "rainDry": 1,
  "hum": 50.4,
  "t": [51.3, 52.54, 52.24, 51.55],
  "tmin": 51.09,
  "tmax": 52.54,
  "spread": 1.45,
  "dt": 0.158,
  "faults": "0x00000000",
  "latched": "0x00000000",
  "reason": "normal",
  "relay": 1,
  "buzzer": 0,
  "ver": "1.0.0"
}
```

Non-finite floats are emitted as `null`. The gateway adds `device_id`, `profile`,
`location` and `received_at` before publishing.

## InfluxDB schema

Measurement: `battery_telemetry`.

Tags: `device_id`, `profile`, `location`, `state`, `ver`.

Fields include: `sequence`, `device_ms`, `risk`, `rule`, `ml_p_unsafe`,
`ml_label`, `ml_valid`, `ml_low_confidence`, `voltage_v`, `current_a`, `power_w`,
`rain_pct`, `rain_dry`, `humidity_pct`, `t0`..`t9`, `t_min`, `t_max`,
`t_spread`, `dt_c_per_s`, `faults_active`, `faults_latched`, `reason`,
`relay_allowed`, `buzzer_on`.

Measurement: `battery_status`.

Tags: `device_id`, `profile`, `location`.

Fields: `online`, `last_seen`, `serial_port`.

Example Flux:

```flux
from(bucket: "battery")
  |> range(start: -1h)
  |> filter(fn: (r) => r._measurement == "battery_telemetry")
  |> filter(fn: (r) => r.device_id == "bench-01" and r._field == "risk")
  |> aggregateWindow(every: 1m, fn: mean, createEmpty: false)
```

More queries and optional downsampling tasks are in
`nas/influxdb/queries.flux`.
