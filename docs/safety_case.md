# Safety Case

## Purpose and status

This document describes the safety concept and the evidence that exists for the
V1 reference platform. It is not a certified safety case and does not claim
compliance with IEC 61508, ISO 26262, IEC 60730 or any battery/charger standard.
A qualified assessor must review the final product, enclosure, installation and
evidence for the target market.

## Safety functions

| ID | Function | Implementation |
| --- | --- | --- |
| SF-1 | Open the load path on overtemperature | Rule engine temperature critical/trip, relay/contactor driver |
| SF-2 | Open the load path on over-current | Current absolute trip and critical thresholds |
| SF-3 | Open the load path on reverse polarity | Voltage reverse threshold |
| SF-4 | Open the load path on e-stop/HVIL | Active-low e-stop input, hard trip |
| SF-5 | Fail safe on MCU reset | Relay driver pull-down, relay opens on reset |
| SF-6 | Detect a stalled MCU | IWDG reset, watchdog-reset fault at boot |
| SF-7 | Detect sensor loss | Open/short/stuck/plausibility checks |
| SF-8 | Prevent closure when unsafe | Relay close requires normal/warning, self-test, no e-stop, heartbeat, no hard fault |
| SF-9 | Preserve evidence | Active/latched fault masks, reason code, telemetry, NAS history |

## Safe state

The safe state is **relay/contactor de-energized**. The buzzer is on in
`TRIPPED`, the LED indicates the state, and the fan may remain on for cooling.
The load path must be designed so loss of coil power, loss of MCU power or a
reset opens the contactor. A hardware pull-down on the driver is mandatory.

## Hazard analysis summary

| Hazard | Cause | Consequence | Control | Residual risk |
| --- | --- | --- | --- | --- |
| Thermal runaway | Cell fault, overcharge, external heat | Fire, venting | Temperature warn/crit/trip, rate and spread rules, relay/contactor, fuse | Requires BMS, fusing, enclosure and cell qualification |
| Over-current | Short, stall, load fault | Cable/connector heating | Current warn/crit/trip, fuse, contactor rating | Trip depends on sensor range and calibration |
| Reverse polarity | Miswiring, jump start | Damage, fire | Reverse-voltage trip, MOSFET/TVS, fuse | User wiring error remains possible |
| Sensor loss | Broken probe, corrosion | Undetected heating | Open/short/stuck/plausibility faults, hard trip on no valid temp | Regular maintenance required |
| MCU stall | Firmware fault, EMI, brownout | Loss of protection | IWDG, relay pull-down, boot fault | External hardware protection still required |
| Comms loss | Gateway/NAS outage | Loss of remote visibility | Local control continues, comms warning, relay closure blocked | Local operator must respond |
| Relay weld | Inrush, contact wear | Load cannot be removed | Use rated contactor/relay, precharge, periodic test | Weld detection is not implemented |
| C2 HV exposure | 48 V contact, isolation failure | Shock, arc flash | HV layout, isolation, service disconnect, PPE | C2 is not fabrication-ready until redesigned |
| Rain/condensation | Ingress, seal failure | Short, corrosion | Rain/humidity warning, IP enclosure, conformal coat | Ingress protection depends on assembly |
| Cold-chain excursion | Door open, compressor fault | Product spoilage | Temperature thresholds, door/humidity inputs, buzzer | Independent refrigeration alarm recommended |

## Assumptions

- The battery has its own BMS, fuse and, where required, a service disconnect.
- The relay/contactor is rated for the load, inrush and inductive kick.
- The enclosure, wiring, fusing and earthing are installed by a qualified person.
- Sensor placement follows the profile wiring guide.
- The gateway/NAS is on a trusted network or protected by a VPN/TLS proxy.
- The user does not rely on ML as the sole protection layer.

## Independence and defense in depth

The local rule engine and safety controller are independent of the NAS and ML.
The relay path includes a hardware pull-down and the IWDG provides a reset path.
However, the MCU is a single point of failure for the rule engine and sensor
acquisition. For higher integrity, add an independent hardware comparator or a
second supervisor for overtemperature/overvoltage/overcurrent, and a contactor
with a mechanically linked auxiliary contact for weld detection.

## Verification evidence

Existing:

- Host unit tests for filters, profiles, assessment, safety controller, telemetry
  and C++ facade.
- 80-vector Python/C model parity test.
- Firmware builds for all five profiles.
- NAS protocol tests and an end-to-end demo run.

Required before field deployment:

- HIL test with real sensors and a controllable load.
- Fault injection for every sensor and communication path.
- Relay/contactor timing and weld test.
- EMC pre-scan and immunity test.
- Thermal chamber, humidity, vibration and ingress testing.
- Independent review of C2 HV isolation and sensing.
- Field pilot with a documented incident/response process.

## Limitations

- No hardware redundancy for the MCU or sensor bus.
- No relay weld detection.
- Config store is disabled in V1.
- ML is advisory and bench-only.
- C2 48 V sensing is not production-ready due to the INA226 36 V limit.
- No fabrication-ready KiCad/Gerber package.
- No certified functional-safety evidence.

## Incident response

1. De-energize the load and isolate the battery.
2. Preserve the controller, SD/flash logs and NAS telemetry.
3. Record the active/latched fault masks, reason, firmware version, profile and
   serial number.
4. Inspect wiring, fuses, relay/contactor, sensors and enclosure seals.
5. Do not re-energize until the root cause is understood and corrective actions
   are complete.
6. Feed the incident into the FMEA, test plan and model/threshold review.
