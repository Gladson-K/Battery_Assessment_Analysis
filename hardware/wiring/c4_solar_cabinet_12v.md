# C4 Solar Cabinet 12 V Circuit

**Use case:** outdoor 12 V solar cabinet or telecom battery hut. Monitors four
NTC probes, ambient temperature/humidity, rain ingress and a 50 A bus. This
profile uses the `LEAD_12V` / default firmware configuration.

## Block Diagram

```
12 V battery J40 -> F40 20 A -> K40 relay -> J41 load
                         |-> isolated 12 V/5 V -> 5 V -> 3.3 V
4x NTC 10k (PA6, PA7, PB0, PB1) + SHT31 (0x44) + INA226 (0x40) + rain (PA5/PB6)
I2C LCD 0x27, fan -> PB11, relay -> PB7, buzzer -> PB10
```

## Connections

| From | To | Notes |
| --- | --- | --- |
| J40 + | F40 input | 20 A fuse |
| F40 output | INA226 shunt IN+ | 1 mOhm, Kelvin |
| Shunt IN- | K40 NO contact | Relay common |
| K40 output | J41 + | Load positive |
| J40 - | J41 - and GND | Star ground |
| 12 V after fuse | Isolated DC-DC input | 5 W, 1000 V isolation |
| DC-DC 5 V | 5 V rail | 10 uF + 100 nF |
| NTC 0 | 10k pull-up to 3.3 V, node PA6 | NTC to GND |
| NTC 1 | 10k pull-up to 3.3 V, node PA7 | NTC to GND |
| NTC 2 | 10k pull-up to 3.3 V, node PB0 | NTC to GND |
| NTC 3 | 10k pull-up to 3.3 V, node PB1 | NTC to GND |
| SHT31 | I2C 0x44 | Shielded from direct sun |
| INA226 | I2C 0x40 | 1 mOhm shunt |
| Rain AO | PA5 | FC-37 module, 3.3 V supply |
| Rain DO | PB6 | Active low on module |
| LCD | I2C 0x27 | Behind cabinet door window |
| Fan | PB11 -> AO3400 -> 12 V | 40 mm, IP68 if exposed |
| Relay coil | PB7 -> 2N7002 -> 12 V | Flyback diode |
| E-stop | J5 -> PA8 | Cabinet door switch |
| MOV1 | 12 V bus to GND | S14K14 |
| D40 | 12 V bus to GND | SMBJ18A |

## Outdoor Installation Rules

1. Use an IP65 or better enclosure with a drip shield and desiccant.
2. Use PG7 glands for all cables; seal unused entries.
3. Fit a 20 A fuse at the battery positive and a 1 A fuse for logic.
4. Use a common-mode choke or ferrite on the solar input and load cables.
5. Mount the NTC probes on the battery negative terminal, charger heatsink,
   cabinet top and cabinet bottom. Use thermal epoxy and strain relief.
6. Keep the rain sensor outside the cabinet, angled so water drains away; do
   not place it where it stays wet after rain.
7. Use conformal coating on the PCB and stainless hardware.
8. Bond the enclosure to earth if a lightning protection system is present.

## Calibration

1. Verify the NTC beta value is 3950 K and nominal 10k at 25 C.
2. Compare each NTC with a reference thermometer at 0 C, 25 C and 50 C.
3. Verify INA226 current with a DC clamp at 5 A and 20 A.
4. Verify the rain sensor dry/wet transition and clean the sensor plate.
5. Verify the relay opens on over-temperature, over-current, door open and
   watchdog reset.

## Safety

- The relay must be rated for the solar array short-circuit current and the
  load's inductive kick.
- Do not rely on the MCU for reverse-polarity protection; use Q1 and a fuse.
- Provide a hardware low-voltage disconnect for the battery if the load can
  deeply discharge it.
- The ML path is disabled for this profile; thresholds are deterministic.
