# PCB Design Guidance

## Status

The repository contains placement plots, schematic-style diagrams and a KiCad
capture guide. It does **not** contain fabrication-ready KiCad schematics, PCB
layouts, footprints or Gerbers. The images in `hardware/diagrams/` are for
engineering review and should be recreated in KiCad before ordering boards.

## Reference stack-up

For the low-voltage profiles (C1, C3, C4, C5):

- 2-layer FR-4, 1.6 mm, 1 oz copper is acceptable for the controller board.
- Use a continuous ground plane under the MCU, crystal, ADC and I2C traces.
- Keep the 3.3 V regulator, crystal and SWD connector near the MCU.
- Keep the relay/contactor driver and its flyback loop away from ADC traces.
- Use a star ground at the power entry and a single-point bond to chassis.

For C2 (48 V), a 4-layer board with a dedicated ground plane is strongly
recommended. The low-voltage and high-voltage areas must be physically
separated with a routed slot or barrier.

## Clearance and creepage

| Domain | Minimum guidance |
| --- | --- |
| 3.3 V logic | 0.2 mm trace/space, normal IPC rules |
| 12 V / 24 V | 0.5 mm spacing, fused input, TVS |
| 48 V C2 | >=8 mm creepage and clearance, 1000 V isolation, HV labels |
| AC mains (not in V1) | Requires a certified supply and creepage/clearance review |

Do not route the 48 V bus on the low-voltage copper. Use a service disconnect,
mid-pack service plug and HV-rated terminals.

## Placement rules by profile

| Profile | Placement priorities |
| --- | --- |
| C1 bench 6 V | LM35/MUX front end near the terminal block; ACS712 away from the relay; LCD and reset button accessible |
| C2 EV 48 V | HV section isolated; precharge resistor and contactor driver separated; MAX31865 array near the PT100 connector; isolation barrier uncrossed |
| C3 UPS 24 V | TMP117 sensors on cell terminals/busbars; MLX90614 aimed at pack top; SHT31 at air inlet; INA226 Kelvin shunt |
| C4 solar 12 V | NTC front end and rain connector protected; INA226 Kelvin shunt; conformal coating; IP65 gland layout |
| C5 cold chain | TMP117 battery/evaporator probes; SHT31 near door seal; ACS712 in compressor supply; LCD visible through door |

Thermal placement:

- Keep the MCU and DC-DC away from the battery exhaust and evaporator outlet.
- Do not place SHT31 in direct cold air or near the DC-DC/relay heat.
- Clamp TMP117/PT100/NTC probes to the measured surface with thermal epoxy and
  strain relief.
- Provide >=10 mm clearance around the buzzer, relay and cable entries.

## Routing

- Route I2C as a pair with a ground reference; keep it short and away from the
  relay/contactor switching node.
- Route SPI2 to the MAX31865 array with a common ground and individual active-low
  chip selects from the 74HC138.
- Keep the ACS712/INA226 analog path away from PWM, relay coils and the fan.
- Use Kelvin connections to the INA226 shunt.
- Put a 100 nF decoupler at every sensor supply pin and 10 uF at each board entry.
- Add series resistors or TVS clamps on external sensor and rain lines.
- Do not share the relay/contactor return current with the ADC ground.

## Connectors and service

Recommended connector set:

| Connector | Use |
| --- | --- |
| J1/J2 | Battery input and load output |
| J3 | SWD programming |
| J4 | USART2 gateway (TX/RX/GND) |
| J5 | E-stop / door switch |
| J6 | I2C expansion |
| J7 | Rain sensor |
| J8 | Temperature probes |
| J9 | Fan |
| J10 | Buzzer |

Add test points for 3.3 V, 5 V, battery voltage, ADC reference, relay drive,
e-stop, UART TX/RX and I2C SDA/SCL.

## Assembly and DFM

1. Use 1 % resistors for dividers and 0.1 % for the MAX31865 reference.
2. Specify the ACS712 sensitivity variant explicitly (05B for C1, 20A for C5).
3. Keep the 2/3 divider on all 5 V ACS712 outputs.
4. Fit a hardware pull-down on the relay/contactor gate so reset is safe.
5. Provide a flyback diode across every relay/contactor coil.
6. Use a fuse on the battery positive and a separate fuse for logic.
7. Add a TVS and reverse-polarity MOSFET at the battery input.
8. Conformal coat C4/C5 boards and use stainless hardware outdoors.
9. Label HV terminals and add a service disconnect on C2.
10. Include a serial number and hardware revision silkscreen.

## C2 high-voltage caveat

The C2 reference design currently lists an INA226. The INA226 VBUS and
current-input common-mode limit is 36 V, so it cannot be connected directly to a
48 V pack. Before fabrication, replace it with an INA228/INA229 (85 V) or an
isolated current-sense amplifier, and measure pack voltage through a 1:11
divider. V1 firmware supports INA226 only; C2 requires a firmware/driver update
and a qualified HV review.

## Fabrication checklist

- [ ] Recreate schematic and PCB in KiCad from the wiring and diagrams.
- [ ] Run ERC/DRC with the target fab's rules.
- [ ] Verify creepage/clearance for C2 and any mains-adjacent wiring.
- [ ] Review the isolated I2C/RS485 boundary and single-point chassis bond.
- [ ] Verify relay/contactor safe state on loss of power and MCU reset.
- [ ] Generate Gerbers, drill files, BOM and pick-and-place.
- [ ] Build a first-article board and execute the HIL test plan.
- [ ] Complete EMC and environmental testing before field deployment.
