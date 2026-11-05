# C3 Server UPS 24 V Circuit

**Use case:** 24 V LFP UPS for a rack or DIN enclosure. Monitors four cell
temperatures, one infrared hotspot, ambient humidity and a 50 A bus.

## Block Diagram

```
24 V DC input J30 -> F30 30 A -> K30 relay -> J31 load
                         |-> isolated 24 V/5 V -> 5 V -> 3.3 V
4x TMP117 (0x48-0x4B) + MLX90614 (0x5A) + SHT31 (0x44) + INA226 (0x40)
I2C LCD 0x27, fan -> PB11, relay -> PB7, buzzer -> PB10
RS485 gateway via isolated transceiver on USART2
```

## Connections

| From | To | Notes |
| --- | --- | --- |
| J30 + | F30 input | 30 A fuse |
| F30 output | K30 NO contact | Relay common |
| K30 output | J31 + | UPS load |
| J30 - | J31 - and GND | Common return |
| F31 1 A | Isolated DC-DC input | Logic supply only |
| DC-DC 5 V | 5 V rail | 10 uF + 100 nF |
| TMP117 0-3 | I2C 0x48-0x4B | Cell terminals and busbars |
| MLX90614 | I2C 0x5A | Aim at pack top, 5-10 cm |
| SHT31 | I2C 0x44 | Enclosure air inlet |
| INA226 | I2C 0x40 | 1 mOhm shunt in + bus |
| INA226 VBUS | After F30 | 24 V bus |
| LCD | I2C 0x27 | 16x2, 5 V backpack |
| Fan | PB11 -> AO3400 -> 5 V or 12 V | 40 mm, 10 CFM minimum |
| Relay coil | PB7 -> 2N7002 -> 5 V | Flyback diode |
| Buzzer | PB10 -> 2N7002 -> 5 V | 85 dB minimum |
| RS485 | USART2 -> isolated transceiver -> J32 | 115200 8N1 |
| E-stop | J5 -> PA8 | Normally closed |

## Layout and Thermal

- Place the board on the intake side of the enclosure; do not put it above the
  battery exhaust.
- TMP117 sensors must be clamped to the cell terminal, not left in air.
- Aim MLX90614 at the center of the pack top; avoid reflective tape and direct
  sunlight.
- Keep the SHT31 away from the DC-DC and relay; it measures ambient air.
- Provide a 40 mm fan inlet and a filtered exhaust. The fan is controlled by
  firmware above 35 C or when the hotspot is 8 C above ambient.
- Use a metal enclosure bonded to protective earth for EMC.

## Calibration

1. Verify 24 V and 5 V rails under load.
2. Compare TMP117 readings with a calibrated thermometer; they should agree
   within +/-0.5 C.
3. Compare INA226 current with a DC clamp meter at 5 A and 20 A.
4. Verify SHT31 humidity in a sealed salt chamber if available.
5. Verify relay dropout and fan start on simulated over-temperature.

## Safety

- The relay must open on critical temperature, over-current, sensor failure and
  watchdog reset.
- If the UPS output is backed by a battery, add a hardware over-voltage and
  under-voltage disconnect independent of the MCU.
- Do not use the 24 V input for mains-powered operation without a certified
  AC-DC supply, fuse and earth bond.
