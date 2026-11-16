# gPace VT-1 firmware (snapshot)

ESP-IDF project for the ESP32-S3 on the VT-1 board (see `../hardware`). Snapshot of the repository `Gpace_VT-1`, committed under Emir Demirhan's account and developed together with İlker Çetinkaya.

```
main/
  main.c              start-up, Bluetooth host task, data notify task, shutdown task
  src/init.c          GPIO, ADC calibration, IMU and GNSS start-up with retries
  src/ubx_gpace.c     u-blox binary protocol: configure, send, read, parse position-velocity-time
  src/imu.c           BMI270 configuration and reads
  src/ble_payload.c   builds the data frame and its checksum
  src/gatt_svc_gpace.c  GATT services (data, device info, battery, firmware transfer)
  src/gap_gpace.c     advertising and connection events
  include/config_file.h  Bosch BMI270 configuration blob
```

Build with ESP-IDF (target `esp32s3`): `idf.py set-target esp32s3 && idf.py build flash monitor`.

The two defects found in review are listed in `../firmware-review/REVIEW.md` and are **not** applied here.
