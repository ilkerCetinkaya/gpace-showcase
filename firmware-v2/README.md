# gPace VT-1 firmware v2

A ground-up rework of the VT-1 firmware, written as if starting the project again with what v1 taught us. The two drivers that took the most effort in v1, the **u-blox GNSS driver** and the **Bosch BMI270 driver**, were written by us from the datasheets and interface descriptions; v2 keeps that work and rebuilds it so it can be tested without hardware.

| | |
|---|---|
| Target | ESP32-S3, ESP-IDF 5.x, NimBLE |
| Language | C11 |
| Wire format | unchanged from v1 (57-byte frame), phone apps keep working |
| Host tests | `cd test/host && make` → 706 checks, 0 failures |
| On hardware | **not yet built or run on a board** |

## Highlights

- **Own u-blox driver** ([`components/gpace_ubx`](components/gpace_ubx)): byte-wise UBX parser with checksum verification, CFG-VALSET/VALGET builders with value width derived from the key ID, ACK collection, NAV-PVT decoding, and a session layer that finds the receiver's baud, configures it (constellations, dynamic model, 25 Hz, automatic output, 460800 baud) and falls back to polling.
- **Own Bosch BMI270 driver** ([`components/gpace_bmi270`](components/gpace_bmi270)): complete init sequence including the 8 KB configuration upload, status verification, single-burst sample reads, gyro cross-axis correction, unit conversion, explicit error codes.
- **Portable core, thin hardware layer:** the drivers talk to function-pointer tables, so a fake GNSS receiver and a fake BMI270 chip run the real code on a laptop.
- **Fixes the v1 defects** found in the code review: gyro Y/Z axes, shutdown queue item size, battery read over-run, work done inside the Bluetooth callback.
- **Same bytes on the wire:** the frame encoder is proven byte-identical to a reproduction of the v1 packer.

## Layout

```
components/   gpace_ubx  gpace_bmi270  gpace_frame  gpace_battery      portable core
              gpace_hal  gpace_power   gpace_snapshot  gpace_ble  gpace_board   ESP-IDF layer
main/         start-up, sensor task, notify task
test/host/    host unit tests with fake receiver and fake IMU
docs/         ARCHITECTURE  PROTOCOL  BRINGUP  MIGRATION  TESTING
```

Build (needs ESP-IDF): `idf.py set-target esp32s3 && idf.py build`. Read [`docs/BRINGUP.md`](docs/BRINGUP.md) first: some register details are marked **verify** because they have not been checked on silicon.

## Honest status

Portable core: written, reviewed, covered by host tests. ESP-IDF layer and `main`: written but never compiled here and never run on a board. Treat it as a reviewed design with a tested core, not as released firmware.

Bosch's configuration blob in `gpace_bmi270_config_blob.h` is third-party data, see [`NOTICE`](NOTICE).
