# Architecture

## System context

```text
Battery / thermal sensors
        |
        v
STM32F411CEU6 controller
  - acquisition, filtering, feature building
  - rule engine + optional on-device RF model
  - safety outputs: relay/contactor, buzzer, fan, LED, LCD
  - CRC-protected JSON telemetry over USART2
        |
        v
Serial gateway / USB-UART or isolated RS485
        |
        v
Mosquitto MQTT -> Ingest -> InfluxDB -> Grafana / FastAPI
```

The controller remains functional without the NAS. The gateway is an observer
and command path, not part of the local trip decision. Loss of gateway
heartbeats is itself a warning condition.

## Repository layout

| Path | Contents |
| --- | --- |
| `firmware/core/` | Portable C99 assessment, profiles, filters, safety controller, telemetry, C++ facade |
| `firmware/ml/` | Deterministic random-forest inference and generated model |
| `firmware/drivers/` | STM32 HAL drivers for sensors and serial I/O |
| `firmware/app/` | Sensor manager and command handler |
| `firmware/src/` | Board bring-up, main loop, interrupt handlers, config store |
| `firmware/tests/` | Host tests and generated model parity vectors |
| `hardware/` | BOM, pin map, wiring, diagrams and KiCad capture guide |
| `nas/gateway/` | UART-to-MQTT gateway, InfluxDB ingester, REST API, simulator |
| `nas/grafana/` | Provisioned datasource template and dashboard |
| `nas/influxdb/` | Flux queries and optional downsampling tasks |
| `docs/` | Engineering, safety, test and product documentation |

## Firmware layers

1. **Board layer** (`firmware/src/board.c`, `firmware/include/board_config.h`)
   configures clocks, GPIO, ADC, I2C, SPI, UART, IWDG and board I/O.
2. **Driver layer** (`firmware/drivers/`) reads LM35, NTC, TMP117, MLX90614,
   SHT31, MAX31865, INA226, INA228, ACS712, rain, LCD and serial frames.
3. **Application layer** (`firmware/app/`) maps a profile to its sensors,
   performs self-test and zero calibration, and handles commands.
4. **Core layer** (`firmware/core/`) is platform-independent C99: filtering,
   feature generation, assessment, safety outputs and telemetry formatting.
5. **ML layer** (`firmware/ml/`) evaluates a compact tree ensemble only when the
   profile and input domain permit it.
6. **Platform entry point** (`firmware/src/main.c`) runs the 1 Hz sample loop,
   10 ms housekeeping loop, watchdog refresh, telemetry and command processing.

The portable core and ML layer are also compiled on the host for unit tests and
for the C++ facade (`firmware/core/BatteryMonitor.hpp/.cpp`).

## Main loop

```text
every 10 ms:
  refresh IWDG
  process one command line
  process reset button edge
  if 1000 ms elapsed:
    read all profile sensors
    compute thermal features and rates
    run ML if enabled and in domain
    update assessment state and faults
    update relay, buzzer, fan, LED and LCD
    emit framed JSON telemetry
```

The relay close has a 1000 ms dwell. A trip opens the relay immediately. Relay
closure also requires: self-test OK, watchdog OK, no e-stop, gateway heartbeat
OK, and state `NORMAL` or `WARNING`.

## Profile selection

`APP_PROFILE_ID` is a compile-time PlatformIO flag. It selects both the sensor
map and the assessment threshold set:

| `APP_PROFILE_ID` | PlatformIO env | Firmware profile |
| --- | --- | --- |
| 1 | `blackpill_f411ce_bench` | `BAT_PROFILE_BENCH_6V` |
| 2 | `blackpill_f411ce_ev_pack` | `BAT_PROFILE_LFP_48V` |
| 3 | `blackpill_f411ce_server_ups` | `BAT_PROFILE_LFP_24V` |
| 4 | `blackpill_f411ce_solar_cabinet` | `BAT_PROFILE_LEAD_12V` |
| 5 | `blackpill_f411ce_cold_chain` | `BAT_PROFILE_COLD_CHAIN` |

Only the bench profile enables ML. The other profiles use deterministic rules
because the model was trained on bench-scale data and would be out of domain on
higher-voltage packs.

## Assessment and safety

The core produces a `bat_assessment_t` with state, active/latched faults,
rule/ML/combined risk, reason code and relay/buzzer permissions. The separate
safety controller adds output interlocks, relay dwell, LED patterns and fan
behavior. See [assessment_logic.md](assessment_logic.md).

## Telemetry

Each sample is serialized to compact JSON, framed as:

```text
$<json>*<4 hex CRC16-CCITT-FALSE>\n
```

The CRC covers only the JSON payload. USART2 at 115200 baud is the gateway
uplink; USART1 is a debug mirror. The frame includes sequence, uptime, state,
risk, ML result, voltage/current/power, rain/humidity, temperature channels,
thermal summary, active/latched fault masks, reason, relay/buzzer states and
firmware version.

## NAS topology

```text
serial-gateway
  - reads devices.yml
  - parses CRC frames
  - publishes battery/<id>/telemetry
  - publishes retained battery/<id>/status
  - sends HEARTBEAT every 5 s
  - writes commands to UART

ingest
  - subscribes to telemetry and status
  - writes InfluxDB points
  - buffers failed writes in SQLite
  - replays buffered points every 10 s

api
  - queries InfluxDB
  - publishes commands to battery/<id>/cmd

grafana
  - reads the provisioned InfluxDB datasource
  - loads the battery dashboard
```

The ingester uses store-and-forward so a short InfluxDB or network outage does
not lose telemetry. The gateway marks a device offline after 30 s without a
valid frame.
