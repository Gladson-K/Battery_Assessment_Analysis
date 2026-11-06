# User Manual

## Controls and indicators

| Item | Location | Meaning |
| --- | --- | --- |
| Status LED | PC13, active-low | Heartbeat, warning, critical or trip pattern |
| Buzzer | PB10 driver | Alarm in warning/critical/trip |
| Relay/contactor | PB7 driver | Energized only when the load path is allowed |
| Fan | PB11 driver | Cooling in warning/critical when the profile uses a fan |
| LCD | I2C 0x27 | State, voltage, current, max temperature, risk and fault flag |
| Reset button | PC14, active-low | Manual reset request; accepted only when safe |
| E-stop | PA8, active-low | Hard trip; opens the relay/contactor |
| USART1 | PA9/PA10 | Debug console |
| USART2 | PA2/PA3 | Gateway/telemetry link |

### LED patterns

| State | Pattern |
| --- | --- |
| `INIT` | Off |
| `NORMAL` | Short heartbeat flash every 2 s |
| `WARNING` | 200 ms flash every 2.4 s |
| `CRITICAL` | 100 ms on/100 ms off |
| `TRIPPED` | Continuously on |

### LCD layout

```text
NORM  6.2V  0.3A
T 45.2C R 12%OK
```

The first line shows state, voltage and current. The second shows maximum
temperature, risk percentage and `OK`/`FLT`. A non-finite value is displayed as
`--`.

## Startup

1. Verify the battery polarity, fuse and e-stop loop.
2. Power the controller; the LED should show the heartbeat after warmup.
3. Confirm the LCD shows plausible voltage/current/temperature.
4. Confirm the gateway receives telemetry and `STATUS`.
5. Run `SELFTEST`; resolve any sensor failure before connecting the load.
6. Run `CAL` with the load disconnected if current zero is out of tolerance.
7. Close the load path only when the state is `NORMAL`/`WARNING` and the relay
   is allowed.

The controller starts in `INIT` for the 3 s warmup. The relay remains open for
at least 1 s after a safe close request.

## Normal operation

- The controller samples once per second and emits telemetry once per second.
- The relay is allowed in `NORMAL` and `WARNING` only when self-test passes,
  the e-stop is released, a gateway heartbeat is present, and no hard trip is
  latched.
- The fan turns on in `WARNING` when risk is high and in `CRITICAL`/`TRIPPED`.
- The buzzer is silent in `NORMAL`, pulses in `WARNING`/`CRITICAL`, and is on in
  `TRIPPED`.
- A gateway heartbeat is expected every 5 s. If heartbeats stop for 15 s, the
  controller raises a comms warning and blocks relay closure.

## Alarms and response

| Indication | Meaning | Operator action |
| --- | --- | --- |
| `WARNING` | A threshold or sensor condition needs attention | Check telemetry, inspect the pack/load, plan shutdown |
| `CRITICAL` | A critical threshold or sensor fault | Disconnect the load, inspect immediately |
| `TRIPPED` | Hard fault/latch; relay open | Isolate the battery, find the cause, then reset when safe |
| Comms warning | Gateway/NAS heartbeat lost | Check UART, gateway container, MQTT and network |
| Sensor open/short | Probe or wiring failure | Repair/replace the probe and wiring |
| Rain/humidity warning | Moisture ingress risk | Inspect enclosure, glands and desiccant |
| ML low confidence | Model outside its reliable region | Treat as advisory; rely on rule state |

## Commands

Commands are sent over USART2 or MQTT/API:

| Command | Action |
| --- | --- |
| `PING` | Liveness check |
| `HEARTBEAT` | Refreshes the gateway heartbeat |
| `STATUS` | Returns the latest telemetry JSON |
| `SELFTEST` | Checks the profile's sensors and buses |
| `CAL` | Calibrates ACS712 zero; load must be disconnected |
| `RESET` | Requests a trip/manual reset when safe |
| `HELP` | Lists commands and syntax |

Do not send `CAL` with the load connected. A `RESET` does not bypass a hard
fault or an active e-stop.

## Maintenance

- Inspect fuses, terminals, relay/contactor and cable glands before each
  deployment and at the interval defined by the site.
- Verify sensor readings against a reference at least annually, or more often
  in harsh environments.
- Replace corroded probes, desiccant and damaged seals.
- Clean the rain sensor plate and check the dry/wet transition.
- Verify the relay/contactor opens on e-stop and reset.
- Check the NAS disk space, backups, container health and certificate expiry.
- Keep firmware version, model checksum, calibration and serial number records.

## Safety notices

- This is not a certified BMS. Use a battery BMS, fuse and independent hardware
  protection appropriate for the installation.
- Do not connect a 48 V C2 pack until the HV sensing redesign and review are
  complete.
- Do not work on a high-voltage pack without PPE, a current-limited supply and a
  second person present.
- Do not rely on the ML result as the only protection layer.
