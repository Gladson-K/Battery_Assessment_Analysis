# C2 EV Pack 48 V Circuit

**Use case:** 48 V LFP traction pack with eight PT100 cell probes, two TMP117
ambient/busbar probes, a 200 A shunt and a main contactor. This is a high-voltage
safety-critical variant and must be reviewed by a qualified HV engineer.

## Block Diagram

```
48 V pack + -> F20 200 A -> K20 contactor -> J21 load
                   |-> R22 100 Ohm precharge -> K20 bypass
                   |-> isolated 48 V/5 V -> 5 V -> 3.3 V
8x PT100 -> 8x MAX31865 -> SPI2 (PB13/14/15) + 74HC138 decoder (PB0-PB2, PB12 enable)
2x TMP117 (0x48/0x49) + INA226 (0x40) on isolated I2C
E-stop/HVIL -> PA8, fan -> PB11, buzzer -> PB10, contactor -> PB7
```

## Connections

| From | To | Notes |
| --- | --- | --- |
| Pack + | F20 input | Class T fuse, 200 A |
| F20 output | K20 contact and R22 | Precharge resistor across K20 |
| K20 output | J21 + | Load positive |
| Pack - | J21 - and GND | Use a chassis stud |
| 48 V (after F20) | Isolated DC-DC input | 10 W module, 1000 V isolation |
| DC-DC 5 V | 5 V rail and AMS1117 | Logic only |
| DC-DC 0 V | Isolated logic GND | Single-point bond to chassis |
| PT100 0-7 | MAX31865 0-7 | 3-wire, RREF 430 Ohm |
| MAX31865 VDD | 3.3 V | 100 nF each |
| MAX31865 CS 0-7 | 74HC138 Y0-Y7 | Active-low outputs |
| 74HC138 A0-A2 | PB0-PB2 | Channel select |
| 74HC138 E1 | PB12 | Active-low enable |
| 74HC138 E2/E3 | GND | Permanently enabled |
| MAX31865 SDI/SDO/CLK | PB15/PB14/PB13 | Shared SPI2 |
| TMP117 SDA/SCL | Isolated I2C | 0x48 and 0x49 |
| INA226 SDA/SCL | Isolated I2C | 0x40, A0/A1 low |
| INA226 IN+ / IN- | Across R20 0.5 mOhm | Kelvin connections |
| INA226 VBUS | Do not connect to 48 V | INA226 is limited to 36 V; use INA228/INA229 or an isolated current-sense amplifier plus a 1:11 pack-voltage divider |
| Contactor coil | PB7 -> gate driver -> 12 V | Flyback diode |
| HVIL loop | J22 -> PA8 | Normally closed |
| E-stop | J5 -> PA8 | Normally closed |
| CAN (V1.1) | TCAN330 -> PB8/PB9 or PA11/PA12 | Not enabled in V1 firmware |
| Fan | PB11 -> MOSFET -> 12 V | 100 nF snubber |

## HV Layout Rules

1. Keep >=8 mm creepage and >=8 mm clearance between 48 V and 3.3 V copper.
2. Use a milled slot or routed barrier between HV and LV areas.
3. Place the isolated DC-DC and ISO1540 on the boundary; do not cross the
   isolation barrier with any other trace.
4. Use 10 AWG or larger for the pack and load conductors; torque terminals to
   the contactor datasheet specification.
5. Add a service disconnect and a mid-pack service plug. Label all HV terminals.
6. The contactor must be normally open and must drop out on loss of coil power.
7. The precharge sequence must be: close precharge path, wait for bus >90% of
   pack voltage, close main contactor, open precharge path.
8. Fit a 5 A fuse in the contactor coil circuit and a 1 A fuse in the logic
   supply.

## Calibration

1. Verify the MAX31865 reference resistors are 430 Ohm 0.1%.
2. Use ice water (0 C) and boiling water (100 C) to verify PT100 readings.
3. Short each PT100 input and confirm the firmware reports `SHORT`; open each
   input and confirm `OPEN`.
4. Verify INA226 current with a calibrated DC load at 10 A, 50 A and 100 A.
5. Verify the contactor opens on e-stop, HVIL open, over-temperature and
   over-current.

## Safety Case

- **HV measurement gap:** the INA226 bus-voltage and current-input common-mode
  limit is 36 V. The 48 V pack must not connect directly to INA226. Replace it
  with an INA228/INA229 (85 V) or an isolated current-sense amplifier and measure
  pack voltage through the 1:11 divider. Firmware V1 supports INA226 only, so C2
  is an engineering reference and is **not fabrication-ready**.
- No ML inference is enabled on this profile; all decisions are deterministic
  threshold and rate-of-change rules.
- The firmware de-energizes the contactor on any critical fault and on watchdog
  reset.
- The contactor coil must have a hardware pull-down so an MCU reset cannot leave
  the contactor energized.
- Do not perform HV testing without a current-limited supply, appropriate PPE,
  and a second person present.
