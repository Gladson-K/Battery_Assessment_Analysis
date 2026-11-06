# Test Plan

## Test levels

| Level | Scope | Environment |
| --- | --- | --- |
| L1 | Portable core, ML parity, C++ facade | Host |
| L2 | Profile builds and static configuration | PlatformIO |
| L3 | Board bring-up and driver tests | Bench hardware |
| L4 | HIL with sensors, load and gateway | Bench/NAS |
| L5 | EMC, environmental, ingress, vibration | Accredited lab |
| L6 | Field pilot | Target installation |

## L1: Host tests

Command:

```bash
make -C firmware/tests clean all test
```

Expected:

```text
test_core: all tests passed
test_model: 80 parity vectors passed
test_cpp: all tests passed
```

Coverage:

- Filter window/median/mean/EMA and clamping.
- Feature building, spread, rate and stuck detection.
- All profile threshold sets and validation.
- Normal, warning, critical and trip transitions; debounce and recovery.
- Temperature, voltage, current, sensor, rain, humidity and comms faults.
- ML unsafe/low-confidence and profile-domain rejection.
- Relay dwell, e-stop, safe state and telemetry framing/CRC.
- C++ facade behavior.

Acceptance: zero failures, zero compiler warnings with `-Wall -Wextra -Werror`.

## L2: Firmware builds

```bash
./scripts/verify.sh
```

The script builds all five PlatformIO environments:

- `blackpill_f411ce_bench`
- `blackpill_f411ce_ev_pack`
- `blackpill_f411ce_server_ups`
- `blackpill_f411ce_solar_cabinet`
- `blackpill_f411ce_cold_chain`

Acceptance: all environments succeed; no new warnings; flash/RAM usage remains
within the STM32F411 limits.

## L3: Board bring-up

For each profile:

1. Current-limited power-up; verify 3.3 V and 5 V rails.
2. Verify SWD programming and boot banner/telemetry.
3. Scan I2C and confirm the expected addresses.
4. Verify each temperature channel with a known reference.
5. Verify voltage divider and current sensor with a calibrated supply/load.
6. Verify relay/contactor opens on reset, e-stop and simulated hard fault.
7. Verify buzzer, LED patterns, fan and LCD.
8. Verify USART2 frames and CRC with the gateway.
9. Run `PING`, `STATUS`, `SELFTEST`, `CAL`, `HELP` and `RESET`.

Acceptance: all channels within the accuracy target; safe state on every fault
injection; no false trips during a 1-hour normal soak.

## L4: HIL and integration

Test matrix:

| Test | Method | Pass criterion |
| --- | --- | --- |
| Temperature warning | Heat probe above warn threshold | Warning after debounce, relay remains allowed |
| Temperature trip | Heat probe above trip threshold | Relay opens within trip debounce + output latency |
| Sensor open | Disconnect probe | Open fault, relay opens |
| Sensor short | Short probe input | Short fault, relay opens |
| Stuck sensor | Hold probe at constant value | Stuck fault, warning floor |
| Over-current | Electronic load above abs max | Relay opens, over-current fault |
| Reverse polarity | Reversed low-voltage supply | Relay stays open, reverse fault |
| E-stop | Open e-stop loop | Relay opens immediately |
| Watchdog | Halt main loop | Reset occurs, relay opens, boot fault reported |
| Comms loss | Stop gateway heartbeats | Warning after 15 s, relay closure blocked |
| NAS outage | Stop InfluxDB/MQTT | Local control continues, ingest buffers/replays |
| Long soak | 72 h normal + 72 h cycling | No lost frames beyond expected, no false trip |

Telemetry checks: sequence continuity, CRC failures, gateway status, InfluxDB
points, Grafana panels, API `/health`, `/devices`, `/latest`, `/history`,
`/alerts` and command publish.

## L5: Environmental and EMC

Plan for the target enclosure:

- ESD contact/air discharge.
- Radiated and conducted emissions.
- Radiated and conducted immunity.
- Burst/EFT, surge and voltage dips on the supply.
- Thermal chamber at the profile's operating range.
- Humidity/condensation and rain ingress (C4/C5).
- Vibration/shock for transport and vehicle use (C2).
- Salt fog/corrosion for outdoor use (C4).
- Isolation and dielectric withstand for C2.

Acceptance criteria must be defined with the target market and test house. This
repository does not contain test reports.

## L6: Field pilot

- Deploy a small number of units with the NAS stack and a support contact.
- Log every fault, reset and command.
- Compare controller readings with independent instruments.
- Review false-positive/false-negative trips weekly.
- Do not enable ML on a new profile without a prospective validation plan.

## Regression rules

Every firmware change must run L1 and L2. Changes to the model require
regenerated parity vectors. Changes to the profile thresholds require updated
tests and documentation. Changes to the NAS schema require protocol tests,
`docker-compose config`, an end-to-end demo run and a dashboard/query review.

## Evidence to retain

- Test command output and firmware size report.
- Model checksum and parity-vector hash.
- Board serial number, profile, firmware version and calibration record.
- HIL logs and fault-injection results.
- EMC/environmental reports.
- NAS backup/restore and upgrade test records.
