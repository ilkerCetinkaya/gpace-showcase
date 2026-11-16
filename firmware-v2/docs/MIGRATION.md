# v1 → v2: what changed and why

| Area | v1 | v2 |
|---|---|---|
| Structure | one `main/` folder, glob build | components with clear dependencies; portable core separated from the chip layer |
| u-blox driver | blocking, long buffer dumps, checksum not verified | byte-wise parser state machine, checksum verified on every message, VALSET value width from the key ID, ACK collection, auto output with poll fallback |
| BMI270 driver | per-sample multi-register reads, 8 KB malloc per burst | one 12-byte burst per sample, no heap use in the data path, chunked upload option, chip-ID and init-status checks with error codes |
| Gyro axes | y and z filled from x | each axis from its own register |
| Gyro cross-axis | not applied | applied with saturation |
| BLE read | ran the sensor read path inside the callback | copies a snapshot |
| Notify | every 10 ms | only when a new frame exists |
| Battery characteristic | sent 6 bytes read from a 4-byte variable | sends exactly 1 byte |
| Shutdown queue | 4-byte items, 1-byte data | 1-byte items |
| LED pins | literals in several files | named in one header |
| Missing symbol | `extern gptimer_handle_t gptimer_main` never defined | removed |
| Tests | none | 700+ host checks with fake receiver and fake IMU |

Frame bytes are unchanged, so existing phone apps keep working.
