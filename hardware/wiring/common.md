# Common Wiring and Interface Rules

All profiles share the STM32F411CEU6 pin map in `pin_map.csv`. This file defines
the interface conventions that every circuit variant must follow.

## Power Rails

| Rail | Source | Loads | Protection |
| --- | --- | --- | --- |
| VBAT | Battery input J1 | DC-DC, relay coil | Fuse F2, reverse MOSFET Q1, TVS D1 |
| 5V | DC-DC or USB-C | LCD, buzzer, fan, ACS712, relay coil | PTC F1, 10 uF + 100 nF |
| 3V3 | AMS1117-3.3 | MCU, I2C sensors, analog references | 10 uF + 100 nF at every sensor |

Keep the analog ground return separate from the relay/contactor return and join
them at the battery negative terminal. Route the ADC traces away from the relay
coil, buzzer and DC-DC switching node.

## I2C Bus

| Signal | MCU pin | Pull-up | Notes |
| --- | --- | --- | --- |
| SCL | PB8 | 4.7 kOhm to 3.3 V | 100 kHz standard mode |
| SDA | PB9 | 4.7 kOhm to 3.3 V | 100 kHz standard mode |

Addresses:

- TMP117: `0x48`, `0x49`, `0x4A`, `0x4B` (ADD0/ADD1 strapping)
- SHT31: `0x44` (ADDR low)
- INA226: `0x40` (C3/C4, A0/A1 low)
- INA228: `0x40` (C2, address pins low)
- MLX90614: `0x5A`
- PCF8574 LCD: `0x27` (A0-A2 low)

Use a 3.3 V-compatible LCD backpack. If the backpack has fixed 5 V pull-ups,
fit a BSS138 level shifter or remove the pull-ups and add 4.7 kOhm to 3.3 V.

## UART

| Port | MCU pins | Baud | Connector | Use |
| --- | --- | --- | --- | --- |
| USART1 | PA9/PA10 | 115200 8N1 | J9/J10 | Debug console and flashing |
| USART2 | PA2/PA3 | 115200 8N1 | J6 | Gateway telemetry and commands |

Telemetry frames are `$<json>*<CRC16>\n`. The gateway must validate the CRC and
acknowledge with `HEARTBEAT` at least every 15 s.

## Safety Inputs and Outputs

| Signal | MCU pin | Type | Safe state |
| --- | --- | --- | --- |
| E-stop / interlock | PA8 | NC contact to GND, internal pull-up | Low = trip |
| Relay / contactor | PB7 | Active-high driver | Low = de-energized |
| Buzzer | PB10 | Active-high driver | Low = silent |
| Fan | PB11 | Active-high MOSFET | Low = off |
| Status LED | PC13 | Active-low on Black Pill | High = off |

The relay/contactor driver must be fail-safe: a loss of MCU power, a reset, or a
watchdog timeout must de-energize the coil. Use a pull-down on the gate/base and
a flyback diode across the coil.

## Analog Inputs

| Input | MCU pin | Range | Network |
| --- | --- | --- | --- |
| MUX output | PA0 | 0-1.5 V | LM35 direct, 100 Ohm series |
| Battery voltage | PA1 | 0-3.0 V | 100k/10k divider, 100 nF |
| Current | PA4 | 0-3.3 V | ACS712 output divided 2:3 |
| Rain | PA5 | 0-3.3 V | FC-37 analog output |
| NTC 0-3 | PA6/PA7/PB0/PB1 | 0-3.3 V | 10k NTC low side, 10k pull-up |

Add a 100 nF capacitor from each ADC node to analog ground. Use 1% resistors for
the voltage and current dividers.

## Connector Pinout

J3 (I2C, 4-pin JST-XH): `3V3`, `GND`, `SDA`, `SCL`.

J4 (SPI/decoder, 6-pin JST-XH): `3V3`, `GND`, `SCK`, `MISO`, `MOSI`, `CS_EN`.

J5 (E-stop, 2-pin JST-XH): `PA8`, `GND`; the switch is normally closed.

J6 (gateway, 4-pin JST-XH): `3V3`, `GND`, `TX`, `RX`; use an isolated RS485 or
USB-UART adapter for long cable runs.

J7 (fan, 2-pin JST-XH): `5V` or `12V`, switched low-side return.

J8 (buzzer, 2-pin JST-XH): `5V`, switched low-side return.

## Harness and Enclosure

- Use 22 AWG for signal wiring and 18 AWG for fan/relay coils.
- Use 10 AWG or larger for C2/C3/C4 battery and load conductors.
- Add strain relief and ferrules at every terminal.
- Keep the temperature sensor cables twisted and shielded where the contactor
  or compressor is nearby.
- Label every connector with the profile, voltage and polarity.
