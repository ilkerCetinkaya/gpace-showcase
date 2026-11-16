# Testing

All tests run on a laptop; no board needed.

```bash
cd firmware-v2/test/host
make
```

Built with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror` and AddressSanitizer plus UBSan.

| Suite | What it proves |
|---|---|
| `test_ubx` | poll frame against the datasheet example, VALSET value sizes, key sizes from key IDs, parser skipping NMEA noise and reading split frames, checksum errors with recovery, oversize rejection, NAV-PVT and ACK/VALGET decoding |
| `test_session` | against a **fake GNSS receiver**: baud scan, no-receiver failure, configuration with exact value sizes, rejected keys reported, automatic stream from chunked reads, poll mode |
| `test_bmi270` | against a **fake BMI270 chip model**: datasheet init sequence, blob size, chunked upload at the right addresses, odd chunk size rejected, failure reports, burst read decoding, cross-axis correction, unit conversion |
| `test_frame` | byte-identical to a reproduction of the v1 packer over 500 random inputs, decode round trip, damage detection, no-fix frames |
| `test_battery` | voltage mapping and saturation |

**Not covered:** everything in `gpace_hal`, `gpace_power`, `gpace_ble` and `main`. That code needs the ESP-IDF toolchain and a board.
