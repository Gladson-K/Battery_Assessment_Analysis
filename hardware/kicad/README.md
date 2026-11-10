# KiCad Capture and Layout Guide

The V1 hardware package defines five circuits and their PCB placement. This
folder is the capture target for the production KiCad project. The generated
diagrams in `../diagrams/` are the design intent; the KiCad files must be
captured, ERC/DRC-checked and reviewed before fabrication.

## Project Hierarchy

```
battery_safety_controller.kicad_pro
├── common_controller.kicad_sch      # MCU, power, safety, connectors
├── c1_bench_6v.kicad_sch            # LM35 mux, ACS712, rain, LCD
├── c2_ev_pack_48v.kicad_sch         # PT100/MAX31865, TMP117, INA228, HV
├── c3_server_ups_24v.kicad_sch      # TMP117, MLX, SHT31, INA226, relay
├── c4_solar_cabinet_12v.kicad_sch   # NTC, SHT31, INA226, rain, relay
└── c5_cold_chain_12v.kicad_sch      # TMP117, SHT31, ACS712, relay
```

Each profile sheet is selected by a population variant. Only one profile sheet
is fitted per assembly. The common controller sheet is always fitted.

## Schematic Capture Notes

- Use `power:GND` and a single `+3V3` symbol; do not create multiple grounds.
- Use the STM32F411CEU6 symbol with the exact pin map from
  `../wiring/pin_map.csv`.
- Mark the 48 V domain with a separate `+48V` power symbol and an isolation
  barrier. Use `ISO1540` and an isolated DC-DC on C2 only.
- Add test points for 5 V, 3V3, GND, PA0, PA1, PA4, PA5, PB8, PB9, PA8 and the
  relay gate.
- Add a no-fit option for the ML model? The model is compiled into flash; no
  hardware option is needed.
- Add a footprint for a 4-pin SWD header and a USB-C service port.
- Add a 2-pin jumper to disable the relay coil for bench testing.

## Footprints

| Ref | Footprint | Notes |
| --- | --- | --- |
| U1 | `Module:BlackPill_STM32F411CE` or 2x20 2.54 mm sockets | Verify Black Pill pinout |
| U2 | `Package_TO_SOT_SMD:SOT-223-3_TabPin2` | AMS1117-3.3 |
| U10-U17 | `Package_TO_SOT_THT:TO-92_Inline` | LM35, 2.54 mm pitch |
| U20 (x8) | `Package_SO:SSOP-20_3.9x8.7mm_P0.635mm` | MAX31865 |
| U24 | `Package_SO:VSSOP-10_3.0x3.0mm_P0.5mm` | INA228 (C2, 85 V) |
| U30-U33 | `Package_DFN_QFN:DFN-8-1EP_2x2mm_P0.5mm_EP0.8x1.6mm` | TMP117 / SHT31 |
| U40 | `Package_TO_SOT_THT:TO-39-4` | MLX90614 |
| U50 | `Package_SO:SOIC-8_3.9x4.9mm_P1.27mm` | ACS712 |
| J1/J2 | `TerminalBlock:TerminalBlock_bornier-2_P5.08mm` | Battery/load |
| J3-J8 | `Connector:JST_XH_B4B-XH-A_1x04_P2.50mm` | Sensors |
| K1 | `Relay:Relay_SPDT_...` or external contactor pads | Verify coil current |
| F1/F2 | `Fuse:Fuse_1206_3216Metric` / `TerminalBlock` | Per profile |

## Stackup

- 4 layers: signal / ground / power / signal.
- 1.6 mm FR4, 1 oz copper, ENIG finish.
- Minimum trace/space 0.20 mm; minimum drill 0.30 mm.
- Keep a continuous ground plane under the MCU and analog front end.
- Use a separate analog island for the ADC divider network and join it at one
  point to the main ground.
- For C2, use a 2.0 mm routed slot under the isolation barrier and no copper
  across it.

## DRC Rules

| Rule | Value |
| --- | --- |
| Minimum clearance (LV) | 0.20 mm |
| Minimum clearance (C2 HV) | 8.0 mm |
| Minimum creepage (C2 HV) | 8.0 mm |
| Minimum trace width (signal) | 0.20 mm |
| Minimum trace width (5 V) | 0.50 mm |
| Minimum trace width (battery/load) | 3.0 mm or copper pour |
| Minimum annular ring | 0.15 mm |
| Minimum hole-to-hole | 0.50 mm |
| Silkscreen clearance | 0.15 mm |

For C2, run ERC with a custom HV net class and verify the isolation barrier
with a ruler and a high-voltage test after assembly.

## Board Outline

| Profile | Board size | Mounting | Notes |
| --- | --- | --- | --- |
| Common | 80 x 56 mm | 4x M3 | Fits all profile front ends |
| C1 | 80 x 56 mm | 4x M3 | Bench enclosure |
| C2 | 100 x 70 mm | 4x M3 | Isolation slot, HV creepage |
| C3 | 80 x 56 mm | 4x M3 | DIN/1U carrier |
| C4 | 80 x 56 mm | 4x M3 | Conformal coated |
| C5 | 80 x 56 mm | 4x M3 | Insulated box |

## Assembly Variants

- `C1`: fit LM35 mux, ACS712-05B, rain, LCD header.
- `C2`: fit MAX31865 array, 74HC138, TMP117, INA228, ISO1540, HV connector,
  contactor driver, precharge pads.
- `C3`: fit TMP117 x4, MLX90614, SHT31, INA226, relay, fan, LCD, RS485.
- `C4`: fit NTC networks, SHT31, INA226, rain, relay, fan, LCD, MOV/TVS.
- `C5`: fit TMP117 x2, SHT31, ACS712-20A, relay, fan, LCD.

## Before Fabrication

1. Capture the common sheet and the selected profile sheet.
2. Run ERC and resolve all warnings.
3. Assign footprints and verify the 3D model.
4. Run DRC with the HV net class for C2.
5. Generate Gerbers, drill file, BOM and pick-and-place.
6. Perform a DFM review with the PCB house.
7. Build one prototype, power it with a current-limited supply, and run the
   firmware self-test before connecting a battery.
