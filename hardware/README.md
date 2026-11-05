# Battery Safety Controller Hardware Package

This directory contains the V1 hardware design package for the STM32F4 battery
safety controller family. It covers the common controller architecture, five
application circuits, bill of materials, wiring tables, and generated
engineering placement diagrams.

## Deliverables

| Area | Files | Status |
| --- | --- | --- |
| Common architecture | `README.md`, `diagrams/system_architecture.*` | Complete |
| Circuit variants | `wiring/c1` ... `wiring/c5`, `diagrams/schematic_*` | Complete |
| PCB placement | `diagrams/pcb_layout_*`, `kicad/README.md` | Complete |
| Enclosure placement | `diagrams/placement_*`, `diagrams/enclosure_section.*` | Complete |
| BOM | `bom/bom_all_profiles.csv` | Complete |
| Pin map | `wiring/pin_map.csv` | Complete |
| KiCad project | `kicad/README.md` | Capture-ready design intent |

The SVG, PNG and PDF diagrams are generated from
`diagrams/generate_diagrams.py`. The KiCad folder describes the intended
schematic hierarchy and footprint rules for layout capture.

## Profile Matrix

| ID | Profile | Battery | Temp front end | Current/voltage | Environment | Output |
| --- | --- | --- | --- | --- | --- | --- |
| C1 | Bench 6 V | 6 V SLA/LFP | 8x LM35 via 74HC4067 | 1:11 divider + ACS712-05B | Bench | Relay, buzzer, LED, LCD |
| C2 | EV pack 48 V | 48 V LFP | 8x PT100 via MAX31865 + 2x TMP117 | INA226, 0.5 mOhm shunt | IP67, HV | Contactor, precharge, buzzer |
| C3 | Server UPS 24 V | 24 V LFP | 4x TMP117 + MLX90614 + SHT31 | INA226, 1 mOhm shunt | 1U/DIN | Relay, fan, LCD |
| C4 | Solar cabinet 12 V | 12 V lead/LFP | 4x NTC 10k B3950 | INA226, 1 mOhm shunt | IP65 outdoor | Relay, fan, LCD |
| C5 | Cold chain 12 V | 12 V LFP | 2x TMP117 + SHT31 | 1:11 divider + ACS712-20A | IP54 insulated | Relay, fan, LCD |

## Common Controller

- **MCU:** STM32F411CEU6 Black Pill, 100 MHz, 512 KB flash, 128 KB RAM.
- **Logic:** 3.3 V; all 5 V inputs divided or level shifted.
- **Power tree:** battery input -> fuse -> reverse-polarity MOSFET -> TVS ->
  isolated or non-isolated DC-DC to 5 V -> 3.3 V LDO -> MCU and sensors.
- **Safety:** internal IWDG, hardware e-stop input, relay/contactor safe state,
  sensor open/short detection, CRC-protected telemetry.
- **Local feedback:** status LED, active buzzer, 16x2 I2C LCD (profiles C1, C3,
  C4, C5).
- **Uplink:** USART2 at 115200 baud, framed JSON telemetry; isolated RS485 or
  USB-UART gateway for deployment.
- **Service:** USART1 debug console, SWD programming, reset button, e-stop.

## Power and Isolation Rules

1. Keep the 3.3 V logic domain separated from battery negative transients.
2. Use an isolated DC-DC for C2 (48 V) and recommended for C4/C5 long cables.
3. Use a 3.3 V-compatible I2C expander or level shifter for 5 V LCD modules.
4. Divide every 0-5 V analog signal to <=3.3 V before the ADC.
5. Add 100 nF at every sensor supply pin and 10 uF at each board entry.
6. C2 requires >=8 mm creepage, 1000 V isolation, HV labels and a service
   disconnect. Do not route the 48 V bus on the low-voltage copper.
7. Fuse the battery positive; switch the load in the return or high side only
   with a properly rated contactor/MOSFET and flyback protection.
8. Use a precharge resistor and contactor sequence for C2 before connecting the
   main load.

## Analog Scaling

| Signal | Network | ADC transfer |
| --- | --- | --- |
| LM35 | Direct, 0-1.5 V | `T = V * 100` |
| NTC 10k | 10k series to 3.3 V | Beta equation, `Rntc = 10k * raw/(4095-raw)` |
| Voltage divider | 100k/10k (1:11) | `Vbat = Vadc * 11` |
| ACS712-05B | 2/3 divider | `I = (Vadc - zero) / (0.1233 * 2/3)` |
| ACS712-20A | 2/3 divider | `I = (Vadc - zero) / (0.100 * 2/3)` |
| Rain analog | Direct 0-3.3 V | 10-bit value = raw12 >> 2 |
| INA226 | I2C | Bus voltage and shunt current |
| MAX31865 | SPI | PT100 resistance and CVD conversion |

The firmware calibrates the ACS712 zero point at boot with `CAL` or the startup
routine. Calibrate only with the load disconnected.

## Connectors

| Connector | Type | Use |
| --- | --- | --- |
| J1 | 2-pin 5.08 mm | Battery input |
| J2 | 2-pin 5.08 mm | Load output |
| J3 | 4-pin JST-XH | I2C sensors and LCD |
| J4 | 6-pin JST-XH | SPI MAX31865 bus and decoder |
| J5 | 2-pin JST-XH | E-stop loop |
| J6 | 4-pin JST-XH | UART gateway (3.3 V/isolated) |
| J7 | 2-pin JST-XH | Fan |
| J8 | 2-pin JST-XH | Buzzer |
| J9 | 4-pin SWD | Programming and debug |
| J10 | USB-C | 5 V service power and debug UART |

## Thermal Placement Rules

- Place the temperature sensor on the cell terminal or busbar, not on the PCB
  edge. Use thermal epoxy or a spring clamp.
- Keep the sensor cable away from the contactor coil and switching nodes.
- For C2, place PT100 sensors on the four hottest cells and the two end cells;
  add TMP117 on the busbar and ambient.
- For C3, place TMP117 between cells, MLX90614 aimed at the top of the pack, and
  SHT31 at the enclosure air inlet.
- For C4, place NTCs on the battery negative terminal, charger heatsink, cabinet
  top and bottom; shield the rain sensor from direct sun.
- For C5, place TMP117 on the battery and evaporator outlet, SHT31 near the door
  seal, and keep the LCD visible through the door.
- Provide >=10 mm clearance around the buzzer and relay for cable routing.

## Assembly and Test

1. Inspect polarity, fuse rating and TVS orientation before power-up.
2. Bring up the 3.3 V rail with a current-limited supply; verify 3.30 V +/-5%.
3. Flash the matching PlatformIO environment from `firmware/platformio.ini`.
4. Run the host tests: `make -C firmware/tests clean all test` (or the
   individual test binaries).
5. Power the board from a current-limited battery simulator.
6. Run `PING`, `STATUS`, `SELFTEST` and `CAL` over USART2.
7. Verify relay opens on e-stop, over-temperature, sensor open and watchdog
   reset. Verify the buzzer and LED patterns.
8. Run a 24-hour soak with the gateway and confirm telemetry continuity.

## Generated Diagrams

Regenerate all engineering images with:

```bash
python3 hardware/diagrams/generate_diagrams.py
```

Outputs include:

- `system_architecture.{svg,png,pdf}`
- `schematic_c1_bench_6v.{svg,png,pdf}` ... `schematic_c5_cold_chain.*`
- `placement_c1_bench_6v.{svg,png,pdf}` ... `placement_c5_cold_chain.*`
- `enclosure_section.{svg,png,pdf}`
- `wiring_c1_bench_6v.{svg,png,pdf}`

## Safety Notice

This is a prototype design package, not a certified product. C2 (48 V) and any
mains-powered variant require a qualified electrical engineer, creepage and
clearance review, fusing coordination, and independent safety validation. Do
not connect a high-voltage battery until the contactor, precharge and isolation
designs have been reviewed and bench-tested with a current-limited source.
