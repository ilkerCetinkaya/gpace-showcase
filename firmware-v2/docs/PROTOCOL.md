# Protocol reference

## BLE data frame (57 bytes, little-endian)

| Offset | Size | Field | Unit |
|---|---|---|---|
| 0 | 1 | sync marker, always `1` | |
| 1 | 4 | longitude | 1e-7 deg |
| 5 | 4 | latitude | 1e-7 deg |
| 9 | 4 | height above ellipsoid | mm |
| 13 | 4 | height above mean sea level | mm |
| 17 | 4 | horizontal accuracy | mm |
| 21 | 4 | ground speed | mm/s |
| 25 | 4 | heading of motion | 1e-5 deg |
| 29 | 4 | speed accuracy | mm/s |
| 33 | 2 | position DOP | 0.01 |
| 35 | 1 | satellites used | |
| 36 | 1 | second | |
| 37 | 1 | minute | |
| 38 | 4 | GPS time of week | ms |
| 42 | 6 | acceleration x, y, z | int16 raw (±8 g in the default config) |
| 48 | 6 | angular rate x, y, z | int16 raw (±2000 °/s in the default config) |
| 54 | 1 | fix type | |
| 55 | 2 | checksum A, B | Fletcher-8 over bytes 0..54, seeded with the length |

The checksum is seeded with the length (55) as two bytes, low then high, before the body bytes are added. `gpace_frame_decode` verifies it and returns an error on damage.

## u-blox binary messages used

| Message | Use |
|---|---|
| UBX-CFG-VALSET | write configuration keys (RAM layer), baud rate last |
| UBX-CFG-VALGET | probe the link (rate key) and check whether configuration is already applied |
| UBX-ACK-ACK / NAK | per-key acknowledgement; failures are collected in a report |
| UBX-NAV-PVT | 92-byte position, velocity, time solution |

Value sizes are derived from bits 28–30 of each key ID (1, 2, 4 or 8 bytes), so VALSET payloads always carry the right width. Keys written: constellations (GPS, Galileo, BeiDou, GLONASS, QZSS, SBAS), dynamic model, measurement and navigation rate, UBX in/out protocols, NMEA output off, NAV-PVT output on, baud rate.

Default configuration: 25 Hz measurement (40 ms), automotive dynamic model, all constellations, automatic NAV-PVT output, 460800 baud. If automatic output is unavailable the session falls back to polling NAV-PVT.

## GATT server

Unchanged from v1 apart from the battery characteristic.

| Service | Characteristic | Access | Notes |
|---|---|---|---|
| custom (0xFFE0) | data (0xFFE1) | read, notify | the 57-byte frame |
| Device Information (0x180A) | manufacturer, model, serial, firmware version | read | |
| Battery (0x180F) | level (0x2A19) | read | **1 byte, 0–100** (v1 sent 6 bytes read past a 4-byte variable) |
| OTA (128-bit UUIDs) | control, data | control: read/write/notify; data: write | firmware transfer over BLE; green LED while active |
