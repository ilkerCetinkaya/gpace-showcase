# Bring-up checklist

**Status: the ESP-IDF layer in this folder has never been compiled or run on a board.** The portable core is covered by host tests (see [TESTING.md](TESTING.md)). Work through this list on first hardware.

1. Install ESP-IDF 5.x, `idf.py set-target esp32s3`, `idf.py build`. Expect to fix small API mismatches (I2C master driver, ADC calibration scheme, NimBLE headers) on first build.
2. Check [`gpace_board.h`](../components/gpace_board/include/gpace_board.h) against the schematic. Pins were carried over from v1.
3. Power on: the latch must hold. If the board browns out, look at the order in `gpace_power_init`.
4. IMU: expect `gpace_bmi270_init` to succeed (chip ID check, 8192-byte blob upload, status 1). If upload fails, set `upload_chunk_bytes` in the config to enable chunked upload.
5. **Verify against the datasheet**: gyro cross-axis (CAS) scaling `/512`, `INIT_ADDR` register split for chunked upload. They are recalled from the datasheet and unproven on silicon.
6. GNSS: watch the log for the baud that answered and "keys acknowledged". Confirm 25 Hz arrives. **Verify** the automatic NAV-PVT key (`0x20910007`) on the module's firmware version; if rejected the session falls back to polling.
7. BLE: connect with a phone, subscribe, confirm 57-byte frames and that the battery read returns one byte.
8. OTA: run a full transfer and confirm the restart.
9. Pull the antenna: the device must report no-fix frames, then restart only after sustained silence.
