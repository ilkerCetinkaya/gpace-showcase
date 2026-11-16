
#ifndef GATT_SVR_GPACE_H
#define GATT_SVR_GPACE_H

/* Includes */
/* NimBLE GATT APIs */
#include "host/ble_gatt.h"
#include "services/gatt/ble_svc_gatt.h"

/* NimBLE GAP APIs */
#include "host/ble_gap.h"

/* OTA Api*/
#include "esp_ota_ops.h"

/* OTA Structs */

typedef enum {
    SVR_CHR_OTA_CONTROL_NOP,
    SVR_CHR_OTA_CONTROL_REQUEST,
    SVR_CHR_OTA_CONTROL_REQUEST_ACK,
    SVR_CHR_OTA_CONTROL_REQUEST_NAK,
    SVR_CHR_OTA_CONTROL_DONE,
    SVR_CHR_OTA_CONTROL_DONE_ACK,
    SVR_CHR_OTA_CONTROL_DONE_NAK,
  } svr_chr_ota_control_val_t;
  

/* Public function declarations */
void send_gps_data(void);
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
void gatt_svr_subscribe_cb(struct ble_gap_event *event);
int gatt_svc_init(void);

#endif // GATT_SVR_H
