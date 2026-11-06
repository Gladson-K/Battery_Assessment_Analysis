# Productization Plan

## Scope

The platform is a reference design and development firmware, not a certified
product. Productization requires hardware review, first-article builds, HIL and
environmental testing, EMC work, supply-chain qualification, and a documented
safety case for the target market.

## Release stages

### 1. Engineering prototype

- Build C1 first; it exercises ADC, mux, I2C, LCD, relay, buzzer and telemetry.
- Bring up C3/C4 next; they reuse I2C/INA226 and deterministic rules.
- Build C5 only after compressor inrush and cold-chamber testing.
- Treat C2 as a separate HV project: replace the INA226 path, complete isolation
  review and obtain a qualified HV sign-off.

### 2. Design verification

- HIL test every profile with simulated and real sensors.
- Run 72-hour and 7-day soak tests with the NAS stack.
- Inject sensor open/short/stuck, e-stop, watchdog reset, brownout, UART noise
  and MQTT/InfluxDB outages.
- Characterize ADC accuracy, relay timing, current calibration and thermal
  response.
- Complete EMC pre-scan and environmental tests for the target enclosure.

### 3. Production readiness

- Freeze the schematic, PCB, BOM and firmware version.
- Release fabrication files with a controlled revision.
- Build a manufacturing test fixture and a calibrated reference load.
- Define serial-number programming and traceability.
- Write the field service and RMA procedure.

## DFM and supply chain

- Prefer parts with multiple distributors and a documented lifecycle.
- Avoid a single-source MCU module if the product volume justifies a direct
  STM32F411 design.
- Specify the ACS712 variant explicitly: 05B for C1, 20A for C5.
- Keep the MAX31865 RREF at 430 Ohm 0.1 % for the PT100 profiles.
- Use 1 % dividers and 0.1 % reference resistors.
- Add a hardware pull-down on relay/contactor drivers.
- Design the enclosure and connector access before the final PCB outline.

## Manufacturing test

Minimum per-board test:

1. Visual inspection, polarity and fuse check.
2. 3.3 V/5 V rail verification under a current-limited supply.
3. MCU programming and SWD ID check.
4. I2C scan for the profile's expected addresses.
5. ADC reference and divider check.
6. Relay/contactor safe-state check on reset and e-stop.
7. Buzzer, LED and fan output check.
8. UART loopback and CRC frame check.
9. Sensor calibration and known-load current check.
10. Serial number, firmware version and test-result record.

Calibration records should include date, fixture ID, operator, reference
instrument, measured values and pass/fail.

## Firmware release process

- Tag every release with a semantic version matching `BAT_FIRMWARE_VERSION`.
- Build all five profiles and archive the `.elf`, `.bin`, `.map` and size report.
- Run host tests and the model parity test.
- Record the model checksum and profile threshold set.
- Keep a signed release note and a rollback image.
- Do not enable the config store in production until it has CRC, A/B slots and a
  migration policy.

## NAS productization

- Change every default password/token in `.env`.
- Enable Mosquitto authentication and TLS or keep port 1883 on a trusted VLAN.
- Put Grafana, InfluxDB and the API behind a reverse proxy with TLS.
- Back up the InfluxDB volume and the SQLite buffer together.
- Add disk-space, container-health and certificate-expiry monitoring.
- Export dashboards and provisioning files as part of the release.
- Pin image tags and test upgrades on a staging NAS before production.

## Certification and standards

The target market determines the required standards. Typical areas to review:

- Electrical safety and isolation for the battery voltage class.
- EMC emissions and immunity for the enclosure and cabling.
- Environmental ingress/thermal/vibration for the installation.
- Functional safety process and documentation if the product is claimed as a
  safety device.
- Battery/charger standards and local installation codes.

Do not claim certification or a safety integrity level until an accredited test
house and a qualified assessor have reviewed the design and evidence.

## Support and lifecycle

- Publish a user manual, wiring diagram, command reference and troubleshooting
  guide.
- Provide a replacement-parts list and a recommended spares kit.
- Define the telemetry retention and privacy policy.
- Track field failures by serial number, firmware version, profile and fault
  mask.
- Feed field data back into the model and threshold review process.
- Set an end-of-life and last-time-buy plan for the MCU, sensors and DC-DC.

## Roadmap

| Priority | Item |
| --- | --- |
| P0 | C2 HV sensing redesign and independent review |
| P0 | Fabrication-ready KiCad files and first-article build |
| P0 | HIL, EMC, environmental and fault-injection evidence |
| P1 | Config store with CRC/A-B slots and signed calibration |
| P1 | Hardware RTC/logging and offline event history |
| P1 | CAN/RS485 multi-drop and device discovery |
| P2 | Additional field-trained models with domain gates |
| P2 | OTA firmware update with rollback |
| P2 | Multi-tenant NAS and role-based access |
