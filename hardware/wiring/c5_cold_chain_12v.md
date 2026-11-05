# C5 Cold Chain 12 V Circuit

**Use case:** 12 V battery-backed cold-chain box or transport refrigerator.
Monitors battery and evaporator temperature, door/ambient humidity, battery
voltage and compressor current.

## Block Diagram

```
12 V battery J1 -> F50 20 A -> K50 relay -> J2 compressor/load
                         |-> isolated 12 V/5 V -> 5 V -> 3.3 V
2x TMP117 (0x48/0x49) + SHT31 (0x44) + ACS712-20A -> 20k/10k -> PA4
Battery voltage -> 100k/10k -> PA1, I2C LCD 0x27, fan -> PB11, relay -> PB7
```

## Connections

| From | To | Notes |
| --- | --- | --- |
| Battery + | F50 input | 20 A fuse |
| F50 output | K50 NO contact | Relay common |
| K50 output | J2 + | Compressor or load |
| Battery - | J2 - and GND | Star ground |
| 12 V after fuse | Isolated DC-DC input | 5 W module |
| DC-DC 5 V | 5 V rail | 10 uF + 100 nF |
| TMP117 0 | I2C 0x48 | Battery terminal |
| TMP117 1 | I2C 0x49 | Evaporator outlet |
| SHT31 | I2C 0x44 | Door seal / cargo air |
| ACS712-20A VCC | 5 V | 100 nF to GND |
| ACS712 OUT | 20k/10k divider | Midpoint to PA4 |
| ACS712 IP+ / IP- | Compressor supply | In series with load |
| Divider top 100k | Battery + after F50 | 1% |
| Divider bottom 10k | GND | Midpoint to PA1 |
| LCD | I2C 0x27 | Visible through door |
| Fan | PB11 -> AO3400 -> 5 V | Internal air circulation |
| Relay coil | PB7 -> 2N7002 -> 12 V | Flyback diode |
| Door switch | J5 -> PA8 | Normally closed when door shut |
| Buzzer | PB10 -> 2N7002 -> 5 V | Door-open and over-temp alarm |

## Cold Chain Rules

1. Keep the battery and electronics in a separate compartment from the
   evaporator to avoid condensation.
2. Use an insulated enclosure with a gasketed door and a desiccant pack.
3. Route the evaporator sensor through a sealed gland; use a strain relief.
4. Place the SHT31 near the door seal but not in the direct cold air stream.
5. Use a 20 A fuse at the battery and a 1 A fuse for logic.
6. Provide a low-temperature cutout independent of the MCU if the battery
   chemistry cannot charge below 0 C.
7. Use a 5 V fan for internal air mixing; do not short-cycle the compressor.
8. Add a door-open buzzer delay so short door openings do not alarm.

## Calibration

1. Verify the 12 V and 5 V rails.
2. Compare TMP117 readings at 4 C and 25 C with a reference thermometer.
3. Verify ACS712 zero with the compressor off, then apply a known 5 A load.
4. Verify SHT31 humidity at ambient and in a sealed humid container.
5. Verify the relay opens on over-temperature, over-current, door open and
   watchdog reset.

## Safety

- The relay must be rated for the compressor inrush current; add a snubber or
  use a solid-state relay if the inrush exceeds the relay rating.
- Do not charge the battery below 0 C unless the BMS explicitly allows it.
- Keep the ML path disabled; this profile uses deterministic thresholds.
