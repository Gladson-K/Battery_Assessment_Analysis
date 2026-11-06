# Documentation Index

This directory contains the engineering and product documentation for the
STM32F4 battery/thermal safety platform. The hardware package is in
`../hardware/`, the NAS stack is in `../nas/`, and the firmware is in
`../firmware/`.

| Document | Purpose |
| --- | --- |
| [architecture.md](architecture.md) | System context, firmware layers, data flow, state machine, NAS topology |
| [assessment_logic.md](assessment_logic.md) | Risk scoring, ML gate, debounce, state transitions, latching and reset |
| [edge_cases.md](edge_cases.md) | Sensor faults, comms loss, watchdog, reset, rain conflict and known limits |
| [model_card.md](model_card.md) | Model provenance, metrics, intended use, limitations and parity evidence |
| [pcb_design.md](pcb_design.md) | Reference PCB rules, placement, isolation, assembly and fabrication gaps |
| [thermal_use_cases.md](thermal_use_cases.md) | C1-C5 use cases, sensor placement and profile thresholds |
| [productization.md](productization.md) | DFM, manufacturing test, calibration, release, certification and roadmap |
| [safety_case.md](safety_case.md) | Hazards, risk controls, FMEA summary and field-validation requirements |
| [test_plan.md](test_plan.md) | Host, firmware, HIL, EMC, environmental and NAS acceptance tests |
| [deployment.md](deployment.md) | Firmware build/flash, calibration, NAS deployment, backup and upgrade |
| [api.md](api.md) | REST API, MQTT topics, command set and telemetry schema |
| [user_manual.md](user_manual.md) | Indicators, controls, alarms, commands and routine maintenance |

Related documents:

- Hardware overview: `../hardware/README.md`
- Pin map: `../hardware/wiring/pin_map.csv`
- Profile wiring: `../hardware/wiring/c1_bench_6v.md` ... `c5_cold_chain_12v.md`
- NAS operations: `../nas/README.md`
- KiCad capture guide: `../hardware/kicad/README.md`

## Status and evidence

Validated in this workspace:

- Host tests: `test_core`, `test_model` (80 parity vectors), `test_cpp`.
- All five PlatformIO firmware environments build.
- NAS protocol tests pass.
- NAS demo stack was started end-to-end: simulator -> Mosquitto -> ingest ->
  InfluxDB -> API/Grafana.

Not yet validated:

- Hardware-in-the-loop testing on assembled boards.
- EMC, environmental, vibration, salt-fog and fault-injection testing.
- Fabrication-ready KiCad schematics, PCB layouts and Gerbers.
- C2 48 V current/voltage sensing (INA226 is limited to 36 V; see the C2
  engineering note).
