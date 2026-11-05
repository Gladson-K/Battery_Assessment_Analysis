# Engineering Diagrams

Generated engineering diagrams for the five battery safety controller profiles.
Each diagram is produced in SVG (editable vector), PNG (documentation) and PDF
(review/print) format.

## Files

| File | Purpose |
| --- | --- |
| `system_architecture.*` | End-to-end sensor, controller, safety, gateway and NAS view |
| `schematic_c1_bench_6v.*` | C1 circuit block schematic and power tree |
| `schematic_c2_ev_pack_48v.*` | C2 HV pack schematic and isolation boundary |
| `schematic_c3_server_ups_24v.*` | C3 UPS schematic |
| `schematic_c4_solar_cabinet_12v.*` | C4 outdoor cabinet schematic |
| `schematic_c5_cold_chain_12v.*` | C5 cold-chain schematic |
| `placement_c1_bench_6v.*` | Bench enclosure and probe placement |
| `placement_c2_ev_pack_48v.*` | EV pack sensor, contactor and HV gland placement |
| `placement_c3_server_ups_24v.*` | 1U/DIN UPS placement and airflow |
| `placement_c4_solar_cabinet_12v.*` | IP65 cabinet placement and cable entries |
| `placement_c5_cold_chain_12v.*` | Insulated cold-chain box placement |
| `enclosure_section.*` | Common enclosure gasket, standoff and airflow section |
| `wiring_c1_bench_6v.*` | C1 point-to-point wiring and pin assignment |

## Regenerate

```bash
python3 hardware/diagrams/generate_diagrams.py
```

Options are defined at the top of the script:

- `PROFILES`: titles, power tree, sensor list, outputs, comms and notes.
- `PLACEMENTS`: enclosure dimensions, battery/PCB/LCD boxes, sensor positions,
  cable glands, airflow arrows and hot zones.

Coordinates are normalized to the enclosure drawing, so a new profile can be
added without touching the rendering code.

## Design Review Checklist

- Every sensor in the firmware profile appears in the schematic and placement.
- Every output (relay/contactor, buzzer, LED, fan, LCD) has a safe state.
- HV and LV areas are physically separated; isolation barriers are unbroken.
- Temperature sensors are on terminals/busbars, not floating in air.
- Airflow enters cool and exits warm; no recirculation over the battery.
- Cable glands, service loops and bend radii are shown.
- The drawing is marked as a prototype design package, not a certified product.
