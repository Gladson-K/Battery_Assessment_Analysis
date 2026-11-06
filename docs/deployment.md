# Deployment Guide

## Prerequisites

- PlatformIO Core or the project virtualenv (`/tmp/opencode/pio-venv/bin/pio`
  in this workspace).
- ST-Link or another supported debug probe for flashing.
- A USB-UART adapter for the gateway link (3.3 V logic).
- Docker and Docker Compose on the NAS.
- A calibrated reference thermometer, DMM and load for commissioning.

## Firmware build

```bash
pio run -d firmware -e blackpill_f411ce_bench
```

Replace the environment for the target:

```text
blackpill_f411ce_bench
blackpill_f411ce_ev_pack
blackpill_f411ce_server_ups
blackpill_f411ce_solar_cabinet
blackpill_f411ce_cold_chain
```

Build output:

```text
firmware/.pio/build/<env>/firmware.elf
firmware/.pio/build/<env>/firmware.bin
firmware/.pio/build/<env>/firmware.map
```

## Flash

```bash
pio run -d firmware -e blackpill_f411ce_bench -t upload
```

Alternative with ST-Link:

```bash
st-flash write firmware/.pio/build/blackpill_f411ce_bench/firmware.bin 0x08000000
```

After flashing:

1. Confirm the LED heartbeat and LCD/telemetry output.
2. Check the firmware version in telemetry (`ver`).
3. Run `PING`, `STATUS`, `SELFTEST` and `HELP` over USART2.
4. Run `CAL` only with the load disconnected, then verify zero current.
5. Apply a known load and verify current/voltage accuracy.

## Board configuration

Profile selection is compile-time via `APP_PROFILE_ID` in
`firmware/platformio.ini`. Pin and scaling constants are in
`firmware/include/board_config.h`. Do not change the profile without rebuilding.

V1 has `APP_ENABLE_CONFIG_STORE=0`; calibration is not persisted. Re-run `CAL`
after every power cycle if the zero point drifts beyond tolerance. Enabling the
store requires a versioned, CRC-protected flash record and a migration policy.

## NAS deployment

```bash
cd nas
cp .env.example .env
# Change INFLUX_PASSWORD, INFLUX_TOKEN, GRAFANA_ADMIN_PASSWORD and API_KEY.
./deploy.sh
```

`deploy.sh` renders `grafana/provisioning/datasources/influxdb.yml` from the
template using the token in `.env`, then starts the stack. The rendered file is
gitignored because it contains a secret.

URLs:

- Grafana: `http://<nas-ip>:3000`
- InfluxDB: `http://<nas-ip>:8086`
- API docs: `http://<nas-ip>:8088/docs`
- API health: `http://<nas-ip>:8088/health`

### Demo without hardware

```bash
cd nas
./deploy.sh --profile demo
```

The simulator publishes a bench-profile stream at 1 Hz. Confirm the Grafana
dashboard and `curl http://localhost:8088/health`.

### Real hardware

1. Connect STM32 USART2 TX/RX/GND to the USB-UART adapter.
2. Confirm the device path (for example `/dev/ttyUSB0`).
3. Edit `nas/gateway/devices.yml` with one entry per controller:

```yaml
devices:
  - id: bench-01
    port: /dev/ttyUSB0
    baud: 115200
    profile: bench_6v
    location: lab
```

4. Set `SERIAL_DEVICE` and `DIALOUT_GID` in `.env` if needed.
5. Start the stack and watch the gateway:

```bash
docker-compose logs -f serial-gateway
docker-compose logs -f ingest
```

The gateway sends `HEARTBEAT` every 5 s. If the firmware has seen a heartbeat
and then loses it for 15 s, it raises a comms warning and blocks relay closure.

### Docker Compose note

This workspace uses `docker-compose` v1.29.2 because the Docker Compose plugin
is not installed. With Docker 29, recreating a service can hit a
`ContainerConfig` compatibility error; remove the affected containers and bring
the stack up again, or install the Compose v2 plugin.

## Calibration

### ACS712 zero

1. Disconnect the load; ensure the sensor is powered and settled.
2. Send `CAL`.
3. Confirm the reported current is near zero.
4. Apply a known load and compare within the profile tolerance.

### Voltage divider

Measure the battery voltage with a calibrated DMM and compare with telemetry.
Adjust `BOARD_VOLTAGE_DIVIDER_RATIO` only if the hardware divider differs from
1:11. The ACS712 has an additional 2/3 divider; firmware already applies
`BOARD_ACS712_DIVIDER_RATIO`.

### Temperature

Use ice water (0 C) and a calibrated reference at 25 C/50 C. For PT100, verify
the 430 Ohm RREF and the 3-wire mode. For NTC, verify beta 3950 and 10k nominal.

## Backup and restore

Back up the InfluxDB volume and the SQLite buffer together:

```bash
docker run --rm -v battery_influxdb-data:/data -v "$PWD/backups:/backup" alpine \
  tar czf /backup/influxdb-$(date +%F).tgz -C /data .
cp nas/data/gateway/buffer.db "nas/backups/buffer-$(date +%F).db"
```

Restore by stopping the stack, restoring the volume and buffer, then starting
the stack and checking `/health` and recent telemetry.

## Upgrade and rollback

1. Build and test the new firmware/NAS release on staging.
2. Back up the NAS data.
3. Flash the controller; keep the previous `.bin` for rollback.
4. Pull/build the new containers and start the stack.
5. Verify `/health`, `/devices`, `/latest`, dashboard panels and command path.
6. If the upgrade fails, restore the previous images/data and reflash the
   previous firmware.

## Operations checklist

- [ ] Passwords/tokens changed from defaults.
- [ ] Mosquitto authentication/TLS or trusted VLAN configured.
- [ ] API key set and stored securely.
- [ ] Reverse proxy/TLS in front of Grafana/InfluxDB/API.
- [ ] Backups scheduled and restore tested.
- [ ] Disk-space and certificate-expiry monitoring enabled.
- [ ] Device IDs and locations documented.
- [ ] Calibration records filed.
- [ ] Firmware version and model checksum recorded.
- [ ] C2 HV review completed before any HV connection.
