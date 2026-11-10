# Edge Cases and Known Limits

## Sensor failures

| Condition | Detection | Response |
| --- | --- | --- |
| Temperature open circuit | Driver status `OPEN`, no valid reading | Critical sensor-open fault; relay opens |
| Temperature short / rail | Driver status `SHORT` or `RAIL` | Critical sensor-short fault; relay opens |
| Stuck temperature | Window has no meaningful change | Stuck fault; warning floor |
| Out-of-range / absent | Driver status or plausibility check | Plausibility fault; warning/critical |
| No valid temperature channel | `temp_valid_mask == 0` | Hard trip, risk 100 |
| Voltage/current `NaN` | Finiteness check | Sensor fault; ML disabled; rule path continues |
| Rain digital/analog conflict | Dry flag with high rain, or wet flag with low rain | Rain-conflict warning |
| Humidity invalid | `humidity_valid == 0` | Humidity contribution is zero, not a false dry reading |

A stuck sensor is considered partially usable so the controller can still make
a conservative decision from the remaining channels. A stuck sensor is never
treated as healthy.

## Communication loss

- The gateway sends `HEARTBEAT` every 5 s.
- The firmware marks `comms_ok` false if a heartbeat has been seen and none
  arrives for 15 s.
- A comms timeout raises a warning and prevents relay closure, but does not
  hard-trip on its own.
- If no gateway has ever been seen, the firmware treats comms as OK so a
  stand-alone controller can run.
- MQTT/LAN loss does not affect the local control loop. The ingester buffers
  failed InfluxDB writes in SQLite and replays them.

## Watchdog and reset

- The IWDG is refreshed every 10 ms main-loop iteration.
- If the loop stalls, the IWDG resets the MCU.
- At boot, an IWDG reset flag raises `BAT_FAULT_WATCHDOG_RESET` for the first
  sample and latches the trip path.
- The relay is de-energized on reset; it does not automatically re-close until
  the state machine reaches a safe normal/warning state and dwell expires.
- Manual reset is ignored while a hard fault, e-stop, self-test failure or
  invalid configuration is present.

## E-stop and safe state

The e-stop input is active-low with a pull-up. An active e-stop is a hard trip
and forces the relay/contactor open. The buzzer is on in `TRIPPED`. The relay
close path also requires a valid heartbeat, self-test and watchdog status.

## Rain and humidity

- The rain digital input is active-low on the module; firmware treats
  `rain_digital_dry == 1` as dry.
- Analog rain percentage and the digital dry flag are cross-checked. A dry flag
  with >=80 % rain, or a wet flag with <=25 % rain, raises a conflict warning.
- Humidity is only used when the SHT31 read is valid. A missing humidity value
  does not create a false dry indication.

## Profile and ML domain

- ML is enabled only for C1 bench 6 V. Other profiles set `ml_enabled = 0`.
- For C1, voltage outside 5.0-8.5 V or current outside -2.0 to 1.5 A raises a
  profile-mismatch fault and the ML result is discarded.
- Low-confidence ML is down-weighted to 20 % and does not by itself force a
  warning; a confident unsafe result is treated as a warning floor.
- The model is not a substitute for the rule engine or a BMS.

## Timing and numeric edge cases

- Milliseconds are kept in `uint32_t`; elapsed-time checks use unsigned
  subtraction, so 49-day wrap is handled.
- Temperature/voltage/current `NaN` values are never converted to plausible
  numbers; telemetry emits `null` for non-finite floats.
- The telemetry payload is bounded by `BAT_TELEMETRY_MAX_PAYLOAD`; if it does
  not fit, the frame is dropped rather than truncated.
- The CRC engine is the same on firmware, gateway and host tests.

## Configuration store

`APP_ENABLE_CONFIG_STORE` is `0` in V1. The build-time profile is authoritative.
The flash store code exists but is not enabled, so calibration and thresholds
do not persist across power cycles beyond the compile-time defaults. A future
release should enable the store with versioned records, CRC and A/B slots.

## Known hardware limits

- **C2 48 V:** the C2 profile uses an INA228 rated to 85 V with a 0.1 mOhm
  Kelvin shunt and a 300 A current design range, so the 48 V bus can be measured
  directly. The HV assembly still needs isolation, creepage/clearance and
  qualified HV approval before fabrication.
- **C5 12 V:** the profile is a 4S LFP/12 V pack with an ACS712-20A sensor. The
  current thresholds are 8/15/20 A and the voltage thresholds are 11.8/10.8 V
  low and 14.4/14.8 V high. Confirm the compressor inrush and relay rating before
  deployment.
- **5 V sensors:** ACS712, LM35 modules and some LCD backpacks need dividers or
  level shifting. Never apply 5 V directly to a 3.3 V ADC pin.
- **No fabrication files:** the KiCad directory is a capture guide, not a
  released schematic/PCB/Gerber package.

## NAS edge cases

- The API requires `X-API-Key` only when `API_KEY` is non-empty.
- Device IDs are restricted to `[A-Za-z0-9_.:-]{1,64}` before being inserted in
  Flux queries.
- InfluxDB downtime does not stop MQTT ingestion; points buffer locally.
- Grafana's datasource file is generated from `.env` by `deploy.sh` and is
  intentionally gitignored because it contains the InfluxDB token.
- The simulator uses profile `demo` and is not started by default.
