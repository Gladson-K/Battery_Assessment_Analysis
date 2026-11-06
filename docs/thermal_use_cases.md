# Thermal Use Cases

## C1: Bench 6 V

**Use case:** laboratory 6 V SLA/LFP pack, charger test rig or student bench.

- Sensors: eight LM35 probes through a 74HC4067 mux, 1:11 battery divider,
  ACS712-05B current sensor, FC-37 rain sensor, 16x2 I2C LCD.
- Focus: cell-to-cell thermal spread, charge/discharge current, bench ML
  demonstration.
- ML: enabled. Domain gate: 5.0-8.5 V, -2.0 to 1.5 A.
- Thresholds: 60/75/85 C; 5.6/5.3 V low; 6.9/7.4 V high; 0.9/1.5/2.0 A.
- Use case: validate the full telemetry, ML and relay path before deploying a
  higher-voltage variant.

## C2: EV Pack 48 V

**Use case:** 48 V LFP traction pack with eight PT100 cell probes, two ambient
probes, a 200 A shunt and a main contactor.

- Sensors: eight MAX31865 3-wire PT100 channels via a 74HC138 decoder, two
  TMP117, INA226 current sense, e-stop/HVIL input.
- Focus: cell temperature, thermal runaway precursors, over-current and
  contactor safe state.
- ML: disabled.
- Thresholds: 55/65/75 C; 42/38 V low; 55/58 V high; 50/100/150 A.
- **Status:** engineering reference only. INA226 is limited to 36 V, so the
  48 V bus cannot connect directly. Use INA228/INA229 or an isolated current
  sensor plus a pack-voltage divider, and obtain an HV review before fabrication.
- Do not perform HV testing without a current-limited supply, appropriate PPE
  and a second person present.

## C3: Server UPS 24 V

**Use case:** rack or DIN-rail 24 V LFP UPS for network/server equipment.

- Sensors: four TMP117 cell/busbar probes, MLX90614 infrared hotspot, SHT31
  ambient humidity, INA226 with a 1 mOhm shunt, LCD, fan.
- Focus: cell imbalance, hotspot detection, humidity/condensation and 50 A bus.
- ML: disabled.
- Thresholds: 55/65/75 C; 22/20 V low; 28.0/29.2 V high; 30/60/100 A.
- Fan turns on in warning/critical and when risk is high. Relay opens on critical
  temperature, over-current, sensor failure and watchdog reset.
- Add an independent hardware UV/OV disconnect if the UPS output is backed by a
  battery.

## C4: Solar Cabinet 12 V

**Use case:** outdoor 12 V solar cabinet or telecom battery hut.

- Sensors: four NTC 10k B3950 probes, SHT31, INA226 with a 1 mOhm shunt, rain
  analog/digital, LCD, fan.
- Focus: outdoor temperature, rain ingress, charger/load current and relay
  control.
- ML: disabled.
- Thresholds: 55/65/75 C; 11.8/10.8 V low; 14.4/15.0 V high; 10/20/30 A.
- Use an IP65 enclosure, PG7 glands, drip shield, desiccant, conformal coating
  and a common-mode choke/ferrite on the solar input.
- The relay must be rated for the solar array short-circuit current and the
  load's inductive kick.

## C5: Cold Chain 12 V

**Use case:** 12 V battery-backed cold-chain box or transport refrigerator.

- Sensors: two TMP117 (battery and evaporator outlet), SHT31 near the door seal,
  ACS712-20A compressor current, 1:11 battery divider, LCD, fan.
- Focus: cargo temperature, evaporator/battery temperature, door/humidity and
  compressor current.
- ML: disabled.
- Thresholds: 8/10/12 C high, 2/0 C low; 11.8/10.8 V low; 14.4/14.8 V high;
  8/15/20 A.
- Keep the battery/electronics in a separate compartment from the evaporator.
  Use a sealed gland for the evaporator sensor and a desiccant pack.
- The relay must handle compressor inrush; use a snubber or solid-state relay if
  the inrush exceeds the relay rating.
- Do not charge the battery below 0 C unless the BMS explicitly allows it.

## Cross-profile thermal guidance

- A single hot sensor is not enough to declare a pack emergency; use the maximum
  channel, spread and rate-of-change together.
- A rising spread at constant average temperature is an early imbalance signal.
- A rapid temperature rise (rate fault) should be treated as a potential internal
  fault even before the absolute trip threshold is reached.
- Place sensors on the measured surface, not in air. Use thermal epoxy and
  strain relief.
- Keep the ML path off for high-voltage and outdoor profiles. The model's
  training domain is bench 6 V.
