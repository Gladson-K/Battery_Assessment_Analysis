# Assessment Logic

## Inputs and features

The feature builder converts raw samples into:

- eight temperature channels (profiles with fewer sensors fill only the active
  channels),
- `t_min`, `t_max`, `t_avg`, `t_spread` and `dt/dt` in C/s,
- battery voltage, current, power and optional resistance,
- rain percentage and dry/wet digital state,
- humidity and validity flags.

The ML feature vector has 12 entries:

```text
[t1, t2, t3, t4, t5, t6, t7, t8, voltage, current, rain_analog, rain_dry]
```

Missing temperature channels are replaced with the average of valid channels
(or 25 C if none are valid). ML is evaluated only when the model self-test
passes, the profile enables ML, and voltage/current are finite.

## Risk score

Each analog quantity is mapped to 0-100 with a two-segment linear scale:

```text
risk_high(value, warn, crit):
  0 .. warn          -> 0 .. 50
  warn .. crit       -> 50 .. 100
  >= crit            -> 100

risk_low(value, warn, crit):
  >= warn            -> 0
  crit .. warn       -> 50 .. 0
  <= crit            -> 100
```

The rule score is a fixed weighted sum:

| Component | Weight |
| --- | --- |
| Maximum temperature risk | 0.32 |
| Thermal spread risk | 0.08 |
| Thermal rate risk | 0.12 |
| Voltage risk | 0.18 |
| Current risk | 0.14 |
| Resistance risk | 0.05 |
| Rain risk | 0.04 |
| Humidity risk | 0.02 |
| Sensor-validity risk | 0.05 |

Rule-score floors are applied for warning, critical and hard-trip conditions.
Hard trips force the rule score and combined risk to 100. Hard trips are:
temperature trip, reverse polarity, absolute over-current, absolute over-voltage,
e-stop, invalid configuration, or no valid temperature channel.

If ML is usable, the combined risk is:

```text
ml_score = clamp(p_unsafe * 100, 0, 100)
ml_weight = 0.50 if confident, 0.20 if low confidence
risk = rule_score * (1 - ml_weight) + ml_score * ml_weight
```

A confident unsafe ML result also raises the combined risk above the warning
threshold. ML never suppresses a rule fault.

## State machine

```text
INIT -> NORMAL       after warmup and no warn/crit condition
NORMAL -> WARNING    warning condition persists for warn_debounce_ms
WARNING -> CRITICAL  critical condition persists for crit_debounce_ms
any -> TRIPPED       hard condition persists for trip_debounce_ms
CRITICAL -> WARNING/NORMAL  after recovery_ms below exit thresholds
WARNING -> NORMAL    after recovery_ms below warning-exit threshold
TRIPPED -> WARNING/NORMAL   only after manual reset and safe conditions
```

Default timing:

| Parameter | Value |
| --- | --- |
| Sample period | 1000 ms |
| Warmup | 3000 ms |
| Warning debounce | 2000 ms |
| Critical debounce | 1000 ms |
| Trip debounce | 500 ms |
| Recovery | 10000 ms |
| Relay close dwell | 1000 ms |
| Gateway heartbeat timeout | 15000 ms |

A trip is latched. Manual reset is accepted only when all hard faults are gone,
at least one temperature channel is valid, self-test passes, e-stop is released,
and risk is below the critical threshold. The reset button is on PC14; the
command interface also supports `RESET`.

## Profile thresholds

| Profile | Temp warn / crit / trip (C) | Voltage high warn / crit (V) | Voltage low warn / crit (V) | Current warn / crit / abs (A) |
| --- | --- | --- | --- | --- |
| C1 bench 6 V | 60 / 75 / 85 | 6.9 / 7.4 | 5.6 / 5.3 | 0.9 / 1.5 / 2.0 |
| C2 EV 48 V | 55 / 65 / 75 | 55 / 58 | 42 / 38 | 50 / 100 / 150 |
| C3 UPS 24 V | 55 / 65 / 75 | 28.0 / 29.2 | 22.0 / 20.0 | 30 / 60 / 100 |
| C4 solar 12 V | 55 / 65 / 75 | 14.4 / 15.0 | 11.8 / 10.8 | 10 / 20 / 30 |
| C5 cold chain 12 V | 8 / 10 / 12 | 14.4 / 14.8 | 11.8 / 10.8 | 8 / 15 / 20 |

Common thresholds: score warn `40`, crit `70`, trip `85`; warning exit `30`;
critical exit `60`; spread/rate limits depend on the profile. C5 also has a low
temperature warning at `2 C` and critical at `0 C`.

C1 additionally uses a 12 C / 20 C spread warning/critical pair and 0.5 / 2.0
C/s rate limits. The ML domain gate for C1 is 5.0-8.5 V and -2.0 to 1.5 A.

## Fault masks

Fault bits cover temperature warning/critical/trip, spread, rate, sensor
open/short/stuck, voltage high/low/reverse/sensor, over-current/current sensor,
rain wet/conflict, ML unsafe/low confidence, comms timeout, invalid config,
watchdog reset, e-stop, fan fault, humidity, resistance, self-test, plausibility,
trip-latched and profile mismatch. The telemetry `reason` field uses a priority
order so the most consequential active fault is reported first.

## Design rules

- The local rule engine is authoritative. ML is advisory.
- A missing temperature channel is not silently ignored; it raises a sensor
  fault, and zero valid channels is a hard trip.
- Voltage, current or humidity values of `NaN` are handled as unavailable, not
  as zero.
- A trip is not cleared by cooling down or by a gateway command alone; a manual
  reset and safe conditions are required.
- The relay is fail-open: loss of MCU power, watchdog reset, e-stop or a critical
  fault de-energizes it.
