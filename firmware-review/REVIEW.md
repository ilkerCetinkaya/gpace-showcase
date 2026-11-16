# Firmware review

Scope: the `main/` folder of the VT-1 firmware snapshot (about 3,000 lines of C). Read line by line; nothing was compiled and nothing was tried on a board.

## Defects (patch provided in `fixes.patch`)

| # | Where | What happens | Fix |
|---|---|---|---|
| 1 | `main/src/ble_payload.c`, in `get_data` | `gyro_y` and `gyro_z` are filled from `imu_res.gyro_x`, so all three angular-rate axes in the data frame carry the X value. Anything computed from the Y and Z rates (turn rate, yaw) is wrong. | Use `gyro_y` and `gyro_z` |
| 2 | `main/main.c`, shutdown queue | The queue is created with 4-byte items (`sizeof(uint32_t)`), but the button interrupt sends a 1-byte variable and the task receives into a 1-byte variable. The queue copies 4 bytes each way, so the receive writes 3 bytes past the variable and the send reads 3 bytes past it (undefined behavior, may corrupt the stack). | Create the queue with `sizeof(uint8_t)` |

Apply from inside `firmware/`:

```bash
git apply -p1 ../firmware-review/fixes.patch
```

## Observations (not patched)

- The Bluetooth device name is a fixed constant (`DEVICE_NAME` in `common.h`), so every unit advertises the same name. A per-unit suffix from the chip's MAC would make units distinguishable.
- `ubx_read_message` does not verify the incoming checksum (the code says so in a comment), so a corrupted GNSS message could be parsed as valid.
- The button interrupt handler sends an event on every edge; no debounce is visible in the handler.
- Start-up prints long buffer dumps while configuring the GNSS receiver; useful during bring-up, noisy in production.
- `config_file.h` contains Bosch's BMI270 configuration blob. Check Bosch's license terms and keep their notice before redistributing it.
- There are no automated tests; the data frame builder and the UBX parser are pure functions and would be easy to cover with host-side tests.
