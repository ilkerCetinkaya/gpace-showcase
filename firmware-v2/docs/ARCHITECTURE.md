# Architecture

v2 splits the firmware into a **portable core** that knows nothing about the chip, and a thin **ESP-IDF layer** that wires it to real pins and radios.

```
            portable core (plain C11, host-tested)                    ESP-IDF layer
 ┌──────────────────────────────────────────────────────┐   ┌─────────────────────────────┐
 │ gpace_ubx          u-blox binary protocol            │   │ gpace_hal    serial + I2C   │
 │   gpace_ubx_session  connect / configure / poll      │◄──┤ gpace_power  latch, button, │
 │ gpace_bmi270       Bosch BMI270 driver               │   │              battery ADC    │
 │ gpace_frame        57-byte BLE frame codec           │   │ gpace_snapshot  latest frame│
 │ gpace_battery      divider + percent math            │   │ gpace_ble    GAP/GATT/OTA   │
 └──────────────────────────────────────────────────────┘   │ gpace_board  pin map        │
                                                            └─────────────────────────────┘
```

## Data flow

```
 GNSS receiver ──serial──► gpace_ubx_poll ─┐
                                           ├─► gpace_frame_from_sensors ─► encode ─► gpace_snapshot_publish
 BMI270 ──I2C──► gpace_bmi270_read_raw ────┘                                              │
                                                                                          ▼
 phone ◄── BLE notify ◄── notify task (only when the sequence number changed) ◄── gpace_snapshot_copy
 phone ──► BLE read ──► read callback copies the snapshot, nothing else
```

Three tasks: the NimBLE host, the **sensor task** (only place that touches the GNSS receiver and the IMU) and a **notify task**.

## The link and bus abstractions

The drivers never call a chip API. They receive a table of function pointers:

| Abstraction | Functions | Why |
|---|---|---|
| `gpace_ubx_link_t` | write, read (with timeout), set baud, flush, now_ms, delay_ms | the same session code runs against a real serial port and against a fake receiver on a laptop |
| `gpace_bmi270_bus_t` | read register block, write register block, delay_us | the same driver runs against the chip and against a fake BMI270 model |

This is what makes 700+ automated checks possible without hardware (see [TESTING.md](TESTING.md)).

## Components

| Component | Depends on ESP-IDF | Role |
|---|---|---|
| `gpace_ubx` | no | UBX framing, Fletcher checksum, CFG-VALSET/VALGET builders, byte-wise parser, NAV-PVT, ACK decoding; session layer for connect, configure, poll |
| `gpace_bmi270` | no | init sequence, config-blob upload, register map, raw read with gyro cross-axis correction, unit conversion |
| `gpace_frame` | no | frame struct, encoder, decoder, checksum, sensor→frame mapping |
| `gpace_battery` | no | pin millivolts → battery millivolts and percent |
| `gpace_hal` | yes | adapters that fill the link and bus tables |
| `gpace_power` | yes | power latch, button interrupt, shutdown sequence, averaged calibrated ADC |
| `gpace_snapshot` | yes (critical section) | latest encoded frame plus a sequence counter |
| `gpace_ble` | yes | GAP advertising, GATT server (data, device info, battery, OTA) |
| `gpace_board` | yes (types only) | every pin and ADC channel in one header |

## Design decisions

- **BLE callbacks never do I/O.** A read of the data characteristic copies the snapshot. In v1 it ran the full sensor read path inside the Bluetooth host task.
- **One producer.** Only the sensor task writes the snapshot; any number of readers copy it under a short critical section.
- **Notify on change.** The notify task sends only when the sequence number moved, instead of every tick.
- **Fail loudly, then recover.** Start-up retries, then a fault LED and a controlled restart; a silent GNSS link also ends in a restart.
- **Wire compatibility.** The 57-byte frame is byte-identical to v1 (proved by a test against a reproduction of the v1 packer), so existing phone apps keep working.
