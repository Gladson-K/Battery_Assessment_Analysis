# Battery Safety Platform

STM32F4 battery and thermal safety controller with deterministic fault handling,
optional on-device machine learning, five hardware profiles, engineering
diagrams, and a NAS-based telemetry platform.

> **Safety notice:** this is a reference design, not a certified BMS or
> functional-safety product. Use a battery BMS, fuse and independent hardware
> protection. C2 48 V sensing now uses an INA228, but the HV assembly still
> requires isolation, creepage and HV approval before fabrication; see
> `docs/safety_case.md` and `hardware/wiring/c2_ev_pack_48v.md`.

## What is included

- **Firmware:** portable C99 core, C++ facade, STM32F4 HAL board layer, drivers,
  profiles, command handler, CRC-protected telemetry, IWDG and config store.
- **ML:** constrained random forest exported to C (`rf-v2-2026`, checksum
  `0x1EF66943`), host parity test with 80 vectors. ML is enabled only on the
  bench 6 V profile.
- **Hardware:** BOM, pin map, wiring guides for C1-C5, system/schematic/
  placement/PCB-layout/enclosure diagrams, and a KiCad capture guide.
- **NAS:** Mosquitto, InfluxDB, Grafana, UART-to-MQTT gateway, store-and-forward
  ingester, FastAPI command/read API and a demo simulator.
- **Documentation:** architecture, assessment logic, edge cases, model card,
  PCB guidance, thermal use cases, safety case, test plan, deployment, API and
  user manual in `docs/`.

## Quick start

### Host tests

```bash
make -C firmware/tests clean all test
```

Expected:

```text
test_core: all tests passed
test_model: 80 parity vectors passed
test_cpp: all tests passed
```

### Firmware build

```bash
pio run -d firmware -e blackpill_f411ce_bench
pio run -d firmware -e blackpill_f411ce_ev_pack
pio run -d firmware -e blackpill_f411ce_server_ups
pio run -d firmware -e blackpill_f411ce_solar_cabinet
pio run -d firmware -e blackpill_f411ce_cold_chain
```

In this workspace the PlatformIO binary is
`/tmp/opencode/pio-venv/bin/pio`.

### Verify everything

```bash
./scripts/verify.sh
```

The script runs host tests, all five firmware builds, NAS protocol tests, Python
syntax checks and a Docker Compose configuration check.

### NAS demo

```bash
cd nas
cp .env.example .env
# Change INFLUX_PASSWORD, INFLUX_TOKEN, GRAFANA_ADMIN_PASSWORD and API_KEY.
./deploy.sh --profile demo
```

Open `http://<nas-ip>:3000` (Grafana), `http://<nas-ip>:8086` (InfluxDB) and
`http://<nas-ip>:8088/docs` (API). The simulator publishes a bench-profile
stream at 1 Hz. See `nas/README.md` for real-hardware setup.

## Profiles

| Env | Use case | Sensors | ML |
| --- | --- | --- | --- |
| `blackpill_f411ce_bench` | C1 bench 6 V | 8x LM35 via MUX, ACS712-05B, rain, LCD | Yes |
| `blackpill_f411ce_ev_pack` | C2 EV 48 V | 8x PT100 MAX31865, 2x TMP117, INA228 | No |
| `blackpill_f411ce_server_ups` | C3 UPS 24 V | 4x TMP117, MLX90614, SHT31, INA226 | No |
| `blackpill_f411ce_solar_cabinet` | C4 solar 12 V | 4x NTC, SHT31, INA226, rain | No |
| `blackpill_f411ce_cold_chain` | C5 cold chain 12 V | 2x TMP117, SHT31, ACS712-20A | No |

Hardware pin map and wiring: `hardware/wiring/pin_map.csv` and
`hardware/wiring/c1_bench_6v.md` ... `c5_cold_chain_12v.md`.

## Repository layout

```text
firmware/   STM32 firmware, portable core, ML, host tests, PlatformIO envs
hardware/   BOM, pin map, wiring, diagrams, KiCad capture guide
model/      training/export pipeline, profile config, generated metrics
nas/        MQTT/InfluxDB/Grafana/API stack and simulator
docs/       engineering, safety, test, deployment and user documentation
scripts/    verification and helper scripts
```

## Telemetry and commands

Serial frame:

```text
$<json>*<4 hex CRC16-CCITT-FALSE>\n
```

Commands: `PING`, `HEARTBEAT`, `STATUS`, `RESET`, `CAL`, `SELFTEST`, `HELP`.
`CAL` must be run with the load disconnected. See `docs/api.md`.

## Validation status

Validated in this workspace:

- Host tests pass.
- All five firmware environments build.
- NAS protocol tests pass.
- NAS demo stack was run end-to-end: simulator -> Mosquitto -> ingest ->
  InfluxDB -> API/Grafana.

Not yet validated:

- Hardware-in-the-loop testing on assembled boards.
- EMC, environmental, ingress, vibration and fault-injection testing.
- Fabrication-ready KiCad schematics, PCB layouts and Gerbers.
- C2 48 V HV assembly, isolation and creepage validation.

## Documentation

Start at `docs/README.md`.

Key documents:

- `docs/architecture.md`
- `docs/assessment_logic.md`
- `docs/edge_cases.md`
- `docs/model_card.md`
- `docs/pcb_design.md`
- `docs/thermal_use_cases.md`
- `docs/safety_case.md`
- `docs/test_plan.md`
- `docs/deployment.md`
- `docs/api.md`
- `docs/user_manual.md`
