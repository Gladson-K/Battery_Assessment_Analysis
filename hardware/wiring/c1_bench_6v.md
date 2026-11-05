# C1 Bench 6 V Circuit

**Use case:** laboratory bench test of 6 V lead-acid or LFP packs with up to eight
temperature probes and a 5 A load. This is the ML-enabled reference build.

## Block Diagram

```
6 V battery J1 -> F2 5 A -> Q1 reverse protection -> K1 relay -> J2 load
                               |-> 6 V to 5 V buck -> 5 V rail -> 3.3 V LDO
8x LM35 -> 74HC4067 -> PA0
Battery voltage -> 100k/10k divider -> PA1
Load current -> ACS712-05B -> 20k/10k divider -> PA4
Rain FC-37 AO -> PA5, DO -> PB6
I2C LCD 0x27, e-stop J5, buzzer J8, UART2 J6
```

## Bill of Materials

Use rows `ALL,*` and `C1,*` from `../bom/bom_all_profiles.csv`.

## Connections

| From | To | Notes |
| --- | --- | --- |
| Battery + | F2 input | 5 A blade fuse |
| F2 output | Q1 source | P-MOSFET reverse protection |
| Q1 drain | K1 NO contact | Relay common |
| K1 NO output | J2 + | Load positive |
| Battery - | J2 - and GND | Star ground |
| 6 V rail | Buck input | Set output to 5.0 V |
| 5 V rail | AMS1117 input | 3.3 V logic |
| LM35 V+ (all) | 5 V | 100 nF each to GND |
| LM35 GND (all) | GND | Keep analog return separate |
| LM35 OUT 0-7 | 74HC4067 Y0-Y7 | 100 Ohm series each |
| 74HC4067 S0-S3 | PB2-PB5 | Channel select |
| 74HC4067 SIG | PA0 | 100 nF to GND |
| 74HC4067 E | GND | Always enabled |
| Divider top 100k | Battery + after F2 | 1% |
| Divider bottom 10k | GND | 100 nF to GND |
| Divider midpoint | PA1 | 1:11 ratio |
| ACS712 VCC | 5 V | 100 nF to GND |
| ACS712 OUT | 20k/10k divider | Midpoint to PA4 |
| ACS712 IP+ / IP- | Load path | In series with J2 + |
| Rain VCC | 3.3 V | FC-37 module |
| Rain AO | PA5 | Optional 100 nF |
| Rain DO | PB6 | Active low on module |
| LCD SDA/SCL | PB9/PB8 | 4.7 kOhm pull-ups |
| LCD VCC | 5 V | PCF8574 backpack |
| E-stop | PA8 to GND | Normally closed |
| Buzzer | PB10 -> 2N7002 -> J8 | Flyback diode |
| Relay coil | PB7 -> 2N7002 -> 5 V | 1N4148 flyback |
| Fan (optional) | PB11 -> AO3400 | Not enabled by C1 firmware |

## Calibration

1. Disconnect the load.
2. Run `CAL` over UART2. The firmware averages 32 samples and stores the ACS712
   zero point.
3. Apply a known 1 A load and verify `STATUS` current within +/-5%.
4. Verify the voltage reading against a DMM at 5.5 V, 6.0 V and 6.5 V.
5. Place LM35 probes in a water bath at 25 C and 60 C; adjust offsets in the
   firmware if needed.

## Safety Checks

- Relay must open on e-stop, over-temperature, over-current and watchdog reset.
- The buzzer must sound continuously in `TRIPPED` state.
- The LCD must show `CRIT` or `TRIP` and the temperature/current that caused it.
- Do not enable ML until the bench profile has been validated with known-good and
  known-bad packs.
