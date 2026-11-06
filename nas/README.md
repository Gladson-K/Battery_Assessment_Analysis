# NAS Data Platform

The NAS stack ingests telemetry from one or more STM32 battery controllers,
stores it in InfluxDB, exposes a REST API, and visualizes it in Grafana. It runs
on any x86-64 or ARM64 NAS with Docker and Docker Compose.

## Services

| Service | Port | Purpose |
| --- | --- | --- |
| Mosquitto | 1883/tcp, 9001/ws | MQTT broker for telemetry and commands |
| InfluxDB 2.7 | 8086 | Time-series storage, 90-day retention by default |
| Grafana 11 | 3000 | Dashboards and alerting |
| serial-gateway | - | Reads STM32 UART frames, publishes MQTT |
| ingest | - | MQTT to InfluxDB with SQLite store-and-forward |
| api | 8088 | Read API and command API |
| simulator | - | Synthetic telemetry for demos (profile `demo`) |

## Quick Start

```bash
cd nas
cp .env.example .env
# edit .env and change INFLUX_PASSWORD, INFLUX_TOKEN and GRAFANA_ADMIN_PASSWORD
./deploy.sh
```

Open:

- Grafana: `http://<nas-ip>:3000` (`admin` / value of `GRAFANA_ADMIN_PASSWORD`)
- InfluxDB: `http://<nas-ip>:8086`
- API docs: `http://<nas-ip>:8088/docs`

### Demo Without Hardware

```bash
cd nas
./deploy.sh --profile demo
```

The simulator publishes a realistic bench-profile stream at 1 Hz. The Grafana
dashboard should show temperature, risk, voltage, current and ML probability.

### Real Hardware

1. Connect the STM32 USART2 to a USB-UART adapter on the NAS.
2. Confirm the device path, for example `/dev/ttyUSB0`.
3. Edit `gateway/devices.yml` with the correct `id`, `port`, `profile` and
   `location`. Add one entry per controller.
4. Set `SERIAL_DEVICE` and `DIALOUT_GID` in `.env` if needed.
5. Run `./deploy.sh` and check `docker-compose logs -f serial-gateway`.

The gateway sends `HEARTBEAT` every 5 s so the firmware keeps `comms_ok` true
and does not latch a gateway-loss fault.

## MQTT Topics

| Topic | Direction | Retained | Payload |
| --- | --- | --- | --- |
| `battery/<id>/telemetry` | controller -> NAS | no | Firmware JSON plus device metadata |
| `battery/<id>/status` | gateway -> NAS | yes | `{"online":true/false,...}` |
| `battery/<id>/cmd` | API/user -> gateway | no | `{"cmd":"STATUS"}` or plain `STATUS` |
| `battery/<id>/ack` | controller -> NAS | no | Command response JSON |

Allowed commands: `PING`, `HEARTBEAT`, `STATUS`, `RESET`, `CAL`, `SELFTEST`,
`HELP`. `CAL` calibrates the ACS712 zero point and must be run with the load
disconnected. `RESET` reboots the controller.

## Telemetry Schema

The firmware emits a compact JSON payload. The ingester maps it to the
`battery_telemetry` measurement.

| JSON key | Influx field/tag | Meaning |
| --- | --- | --- |
| `device_id`, `profile`, `location` | tags | Device identity (added by gateway) |
| `state` | tag | `NORMAL`, `WARNING`, `CRITICAL`, `TRIPPED`, `INIT` |
| `ver` | tag | Firmware version |
| `seq`, `ms` | fields | Sequence and device uptime |
| `risk`, `rule` | fields | Combined and rule-only risk score (0-100) |
| `ml.p`, `ml.label`, `ml.valid`, `ml.lowConf` | fields | On-device ML result |
| `v`, `i`, `p` | fields | Voltage, current, power |
| `rain`, `rainDry`, `hum` | fields | Rain percentage, dry flag, humidity |
| `t0..t9` | fields | Temperature channels |
| `tmin`, `tmax`, `spread`, `dt` | fields | Thermal summary and rate of change |
| `faults`, `latched` | fields | Active and latched fault bitmasks |
| `reason`, `relay`, `buzzer` | fields | Decision reason and output states |

The `battery_status` measurement stores `online`, `last_seen` and `serial_port`
per device. See `influxdb/queries.flux` for ready-to-run Flux queries.

## API

Set `API_KEY` in `.env` to require an `X-API-Key` header. Leave it empty only on
a trusted network.

```bash
curl http://nas:8088/health
curl -H "X-API-Key: $API_KEY" http://nas:8088/devices
curl -H "X-API-Key: $API_KEY" http://nas:8088/devices/bench-01/latest
curl -H "X-API-Key: $API_KEY" "http://nas:8088/devices/bench-01/history?field=risk&minutes=60&window=1m"
curl -H "X-API-Key: $API_KEY" http://nas:8088/alerts

curl -X POST -H "Content-Type: application/json" -H "X-API-Key: $API_KEY" \
  -d '{"cmd":"STATUS"}' http://nas:8088/devices/bench-01/command
```

## Storage and Retention

- InfluxDB retention is set to 90 days at first setup. Change
  `DOCKER_INFLUXDB_INIT_RETENTION` before the first start.
- The ingester buffers failed writes in `data/gateway/buffer.db`. Replay runs
  every 10 s and is safe across restarts.
- Back up the `influxdb-data` volume and `data/gateway/buffer.db` together.
- For long-term storage, add a downsampling task from `influxdb/queries.flux`
  and keep raw data for 30-90 days.

Example backup:

```bash
docker run --rm -v battery_influxdb-data:/data -v "$PWD/backups:/backup" alpine \
  tar czf /backup/influxdb-$(date +%F).tgz -C /data .
cp nas/data/gateway/buffer.db "nas/backups/buffer-$(date +%F).db"
```

## Security Hardening

1. Change every password and token in `.env`.
2. Create a Mosquitto password file and set `allow_anonymous false`:
   ```bash
   docker run --rm -it -v "$PWD/nas/mosquitto:/mosquitto/config" eclipse-mosquitto:2.0.20 \
     mosquitto_passwd -c /mosquitto/config/passwd battery
   ```
3. Set `MQTT_USERNAME` and `MQTT_PASSWORD` in `.env`.
4. Put Grafana, InfluxDB and the API behind a reverse proxy with TLS.
5. Do not expose port 1883 to the internet. Use a VPN or MQTT over TLS.
6. Set `API_KEY` and rotate it periodically.
7. Restrict the `serial-gateway` container to the specific device instead of
   mounting all of `/dev` if your host supports it.

## Operations

```bash
docker-compose ps
docker-compose logs -f ingest
docker-compose logs -f serial-gateway
docker-compose restart api
docker-compose down
docker-compose up -d --build
```

Health checks:

```bash
curl -f http://localhost:8088/health
mosquitto_sub -h localhost -t 'battery/#' -v
```

## Troubleshooting

| Symptom | Check |
| --- | --- |
| No telemetry | `docker-compose logs serial-gateway`; verify `/dev/ttyUSB0` and baud 115200 |
| CRC errors | Verify the firmware and gateway use CRC16-CCITT-FALSE; check cable noise |
| Commands ignored | Check the topic uses the exact device id and the command is in the allowed set |
| Influx write failures | Check `INFLUX_TOKEN`, `INFLUX_ORG` and `INFLUX_BUCKET`; inspect `data/gateway/buffer.db` |
| Grafana shows no data | Confirm the datasource token matches `.env`; rerun `./deploy.sh` |
| Device shows offline | The gateway marks offline after 30 s without a valid frame |

## Files

- `docker-compose.yml`: stack definition.
- `gateway/protocol.py`: frame parser and CRC.
- `gateway/serial_gateway.py`: UART to MQTT.
- `gateway/ingest.py`: MQTT to InfluxDB with buffering.
- `gateway/api.py`: REST API and command publishing.
- `gateway/simulator.py`: synthetic telemetry.
- `grafana/dashboards/battery.json`: provisioned dashboard.
- `influxdb/queries.flux`: example queries and optional tasks.
- `tests/test_protocol.py`: CRC and framing tests.
