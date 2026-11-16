
/* Includes */
#include "gatt_svc_gpace.h"
#include "common.h"
#include "ubx_gpace.h"
#include "ble_payload.h"

/* Private function declarations */
static int gps_data_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);                               

static int manufacturer_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg); 

static int model_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);        

static int serial_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);   

static int firmware_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg);         

static int battery_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);   

static int gatt_svr_chr_ota_control_cb(uint16_t conn_handle, uint16_t attr_handle,
                                       struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svr_chr_ota_data_cb(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg);

static int gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len,
                              uint16_t max_len, void *dst, uint16_t *len);

static void update_ota_control(uint16_t conn_handle);


/* Private variables */

/* Gps data service */
static const ble_uuid16_t gps_data_svc_uuid = BLE_UUID16_INIT(0xFFE0);

static uint8_t gps_data_chr_val[57] = {0};
static uint16_t gps_data_chr_val_handle;
static const ble_uuid16_t gps_data_chr_uuid = BLE_UUID16_INIT(0xFFE1);

static uint16_t gps_data_chr_conn_handle = 0;
static bool gps_data_chr_conn_handle_inited = false;
static bool gps_data_ind_status = false;

/* Device information service */
static const ble_uuid16_t device_info_svc_uuid = BLE_UUID16_INIT(0x180A);

static char manufacturer_chr_val[] = {"Gpace Tech Inc."};
static uint16_t manufacturer_chr_val_handle;
static const ble_uuid16_t manufacturer_chr_uuid = BLE_UUID16_INIT(0x2A29);

static char model_chr_val[] = {"VT-1"};
static uint16_t model_chr_val_handle;
static const ble_uuid16_t model_chr_uuid = BLE_UUID16_INIT(0x2A24);

static char serial_chr_val[] = {"25B1111"};
static uint16_t serial_chr_val_handle;
static const ble_uuid16_t serial_chr_uuid = BLE_UUID16_INIT(0x2A25);

static char firmware_chr_val[] = {"1.0.0"};
static uint16_t firmware_chr_val_handle;
static const ble_uuid16_t firmware_chr_uuid = BLE_UUID16_INIT(0x2A26);

/* Battery Service */

static const ble_uuid16_t battery_svc_uuid = BLE_UUID16_INIT(0x180F);

static int battery_chr_val[] = {0};
static uint16_t battery_chr_val_handle;
static const ble_uuid16_t battery_chr_uuid = BLE_UUID16_INIT(0x2A19);

/* OTA Service */
// d6f1d96d-594c-4c53-b1c6-244a1dfde6d8
static const ble_uuid128_t gatt_svr_svc_ota_uuid =
    BLE_UUID128_INIT(0xd8, 0xe6, 0xfd, 0x1d, 0x4a, 024, 0xc6, 0xb1, 0x53, 0x4c,
                     0x4c, 0x59, 0x6d, 0xd9, 0xf1, 0xd6);

// characteristic: OTA Control
// 7ad671aa-21c0-46a4-b722-270e3ae3d830
static const ble_uuid128_t gatt_svr_chr_ota_control_uuid =
    BLE_UUID128_INIT(0x30, 0xd8, 0xe3, 0x3a, 0x0e, 0x27, 0x22, 0xb7, 0xa4, 0x46,
                     0xc0, 0x21, 0xaa, 0x71, 0xd6, 0x7a);

// characteristic: OTA Data
// 23408888-1f40-4cd8-9b89-ca8d45f8a5b0
static const ble_uuid128_t gatt_svr_chr_ota_data_uuid =
    BLE_UUID128_INIT(0xb0, 0xa5, 0xf8, 0x45, 0x8d, 0xca, 0x89, 0x9b, 0xd8, 0x4c,
                     0x40, 0x1f, 0x88, 0x88, 0x40, 0x23);

uint8_t gatt_svr_chr_ota_control_val;
uint8_t gatt_svr_chr_ota_data_val[512];

uint16_t ota_control_val_handle;
uint16_t ota_data_val_handle;

const esp_partition_t *update_partition;
esp_ota_handle_t update_handle;
bool updating = false;
uint16_t num_pkgs_received = 0;
uint16_t packet_size = 0;


/* GATT services table */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
// 
    {
     // GPS Data Service
     .type = BLE_GATT_SVC_TYPE_PRIMARY,
     .uuid = &gps_data_svc_uuid.u,
     .characteristics =
         (struct ble_gatt_chr_def[]){
             {/* GPS data characteristic */
              .uuid = &gps_data_chr_uuid.u,
              .access_cb = gps_data_chr_access,
              .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
              .val_handle = &gps_data_chr_val_handle},
             {
                 0, /* No more characteristics in this service. */
             }}},
    {
        // Device information service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &device_info_svc_uuid.u,
        .characteristics = 
            (struct ble_gatt_chr_def[]){
                {
                    .uuid = &manufacturer_chr_uuid.u,
                    .access_cb = manufacturer_chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &manufacturer_chr_val_handle},
                {
                    .uuid = &model_chr_uuid.u,
                    .access_cb = model_chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &model_chr_val_handle},

                {
                    .uuid = &serial_chr_uuid.u,
                    .access_cb = serial_chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &serial_chr_val_handle},
                {
                    .uuid = &firmware_chr_uuid.u,
                    .access_cb = firmware_chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &firmware_chr_val_handle},
                {
                    0, /* No more characteristic in this service*/
                }    
                
                }

            },
    {
        // Battery service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &battery_svc_uuid.u,
        .characteristics = 
            (struct ble_gatt_chr_def[]){

                {
                    .uuid = &battery_chr_uuid.u,
                    .access_cb = battery_chr_access,
                    .flags = BLE_GATT_CHR_F_READ,
                    .val_handle = &battery_chr_val_handle},
                {
                    0,
                }
            }
    },

    {
        // service: OTA Service
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &gatt_svr_svc_ota_uuid.u,
        .characteristics =
            (struct ble_gatt_chr_def[]){
                {
                    // characteristic: OTA control
                    .uuid = &gatt_svr_chr_ota_control_uuid.u,
                    .access_cb = gatt_svr_chr_ota_control_cb,
                    .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE |
                             BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_WRITE_NO_RSP,
                    .val_handle = &ota_control_val_handle,
                },
                {
                    // characteristic: OTA data
                    .uuid = &gatt_svr_chr_ota_data_uuid.u,
                    .access_cb = gatt_svr_chr_ota_data_cb,
                    .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
                    .val_handle = &ota_data_val_handle,
                },
                {
                    0,
                }},
    },

    {
        0, /* No more services. */
    },

};

/* Private functions */
static int gps_data_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) {
    /* Local variables */
    int rc;

    /* Handle access events */
    switch (ctxt->op) {

    /* Read characteristic event */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

         if (attr_handle == gps_data_chr_val_handle) {
            /* Update access buffer value */
            
            get_data(gps_data_chr_val); // Get data from other services

            rc = os_mbuf_append(ctxt->om, &gps_data_chr_val,
                                sizeof(gps_data_chr_val));

            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to gps data characteristic, opcode: %d",
        ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;

}

static int manufacturer_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* Local variables */
    int rc;

    /* Handle access events */
    switch (ctxt->op) {

    /* Read characteristic event */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

         if (attr_handle == manufacturer_chr_val_handle) {
            /* Update access buffer value */
            

            rc = os_mbuf_append(ctxt->om, &manufacturer_chr_val,
                                sizeof(manufacturer_chr_val));

            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to gps data characteristic, opcode: %d",
        ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}


static int model_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* Local variables */
    int rc;

    /* Handle access events */
    switch (ctxt->op) {

    /* Read characteristic event */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

         if (attr_handle == model_chr_val_handle) {
            /* Update access buffer value */
            
            rc = os_mbuf_append(ctxt->om, &model_chr_val,
                                sizeof(model_chr_val));

            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to gps data characteristic, opcode: %d",
        ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
    
}

static int serial_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* Local variables */
    int rc;

    /* Handle access events */
    switch (ctxt->op) {

    /* Read characteristic event */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

         if (attr_handle == serial_chr_val_handle) {
            /* Update access buffer value */
            

            rc = os_mbuf_append(ctxt->om, &serial_chr_val,
                                sizeof(serial_chr_val));

            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to gps data characteristic, opcode: %d",
        ctxt->op);

    
    return BLE_ATT_ERR_UNLIKELY;
}

static int firmware_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
/* Local variables */
    int rc;

    /* Handle access events */
    switch (ctxt->op) {

    /* Read characteristic event */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

         if (attr_handle == firmware_chr_val_handle) {
            /* Update access buffer value */
            
            rc = os_mbuf_append(ctxt->om, &firmware_chr_val,
                                sizeof(firmware_chr_val));

            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to gps data characteristic, opcode: %d",
        ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;

}

int battery_chr_access(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    /* Local variables */
    int rc;

    /* Handle access events */
    switch (ctxt->op) {

    /* Read characteristic event */
    case BLE_GATT_ACCESS_OP_READ_CHR:
        /* Verify connection handle */
        if (conn_handle != BLE_HS_CONN_HANDLE_NONE) {
            ESP_LOGI(TAG, "characteristic read; conn_handle=%d attr_handle=%d",
                     conn_handle, attr_handle);
        } else {
            ESP_LOGI(TAG, "characteristic read by nimble stack; attr_handle=%d",
                     attr_handle);
        }

         if (attr_handle == battery_chr_val_handle) {
            /* Update access buffer value */
            adc_get_val(battery_chr_val);
         
            battery_chr_val[0] = ((battery_chr_val[0]-2250)*100/750); // (2250, 3000) -> (0, 100)
            
            if (battery_chr_val[0] > 100)  // Saturate at %100
            {
                battery_chr_val[0] = 100; 
            }
            else if (battery_chr_val[0] < 0) // Saturate at %0
            {
                battery_chr_val[0] = 0;
            }
            
            
            rc = os_mbuf_append(ctxt->om, &battery_chr_val,
                                sizeof(firmware_chr_val));

            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto error;

    /* Unknown event */
    default:
        goto error;
    }

error:
    ESP_LOGE(
        TAG,
        "unexpected access operation to gps data characteristic, opcode: %d",
        ctxt->op);
    return BLE_ATT_ERR_UNLIKELY;
}

int gatt_svr_chr_ota_control_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    uint8_t length = sizeof(gatt_svr_chr_ota_control_val);

    switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
        // a client is reading the current value of ota control
        rc = os_mbuf_append(ctxt->om, &gatt_svr_chr_ota_control_val, length);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        break;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
        // a client is writing a value to ota control
        rc = gatt_svr_chr_write(ctxt->om, 1, length,
                                &gatt_svr_chr_ota_control_val, NULL);
        // update the OTA state with the new value
        update_ota_control(conn_handle);
        return rc;
        break;

        default:
        break;
    }

  // this shouldn't happen
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
    
}

int gatt_svr_chr_ota_data_cb(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    esp_err_t err;
  
    // store the received data into gatt_svr_chr_ota_data_val
    rc = gatt_svr_chr_write(ctxt->om, 1, sizeof(gatt_svr_chr_ota_data_val),
                            gatt_svr_chr_ota_data_val, NULL);
  
    // write the received packet to the partition
    if (updating) {
      err = esp_ota_write(update_handle, (const void *)gatt_svr_chr_ota_data_val,
                          packet_size);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed (%s)!",
                 esp_err_to_name(err));
      }
  
      num_pkgs_received++;
      ESP_LOGI(TAG, "Received packet %d", num_pkgs_received);
    }
  
    return rc;
}

static int gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len, void *dst, uint16_t *len) 
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
    return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
    return BLE_ATT_ERR_UNLIKELY;
    }

    return ESP_OK;
}

static void update_ota_control(uint16_t conn_handle) {

    struct os_mbuf *om;
    esp_err_t err;
  
    // check which value has been received
    switch (gatt_svr_chr_ota_control_val) {
      case SVR_CHR_OTA_CONTROL_REQUEST:
        // OTA request
        ESP_LOGI(TAG, "OTA has been requested via BLE.");
        // get the next free OTA partition
        update_partition = esp_ota_get_next_update_partition(NULL);
        // start the ota update
        err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES,
                            &update_handle);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "esp_ota_begin failed (%s)",
                   esp_err_to_name(err));
          esp_ota_abort(update_handle);
          gatt_svr_chr_ota_control_val = SVR_CHR_OTA_CONTROL_REQUEST_NAK;
        } else {
          gatt_svr_chr_ota_control_val = SVR_CHR_OTA_CONTROL_REQUEST_ACK;
          updating = true;

          gpio_set_level(POWER_LED_GREEN, 0);  // Toggle on green LED to notify user OTA has started
          gpio_set_level(POWER_LED, 1); // Toggle off the blue LED
  
          // retrieve the packet size from OTA data
          packet_size =
              (gatt_svr_chr_ota_data_val[1] << 8) + gatt_svr_chr_ota_data_val[0];
          ESP_LOGI(TAG, "Packet size is: %d", packet_size);
  
          num_pkgs_received = 0;
        }
  
        // notify the client via BLE that the OTA has been acknowledged (or not)
        om = ble_hs_mbuf_from_flat(&gatt_svr_chr_ota_control_val,
                                   sizeof(gatt_svr_chr_ota_control_val));
        ble_gatts_notify_custom(conn_handle, ota_control_val_handle, om);
        ESP_LOGI(TAG, "OTA request acknowledgement has been sent.");
  
        break;
  
      case SVR_CHR_OTA_CONTROL_DONE:
  
        updating = false;
  
        // end the OTA and start validation
        err = esp_ota_end(update_handle);
        if (err != ESP_OK) {
          if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG,
                     "Image validation failed, image is corrupted!");
          } else {
            ESP_LOGE(TAG, "esp_ota_end failed (%s)!",
                     esp_err_to_name(err));
          }
        } else {
          // select the new partition for the next boot
          err = esp_ota_set_boot_partition(update_partition);
          if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!",
                     esp_err_to_name(err));
          }
        }
  
        // set the control value
        if (err != ESP_OK) {
          gatt_svr_chr_ota_control_val = SVR_CHR_OTA_CONTROL_DONE_NAK;
        } else {
          gatt_svr_chr_ota_control_val = SVR_CHR_OTA_CONTROL_DONE_ACK;
        }
  
        // notify the client via BLE that DONE has been acknowledged
        om = ble_hs_mbuf_from_flat(&gatt_svr_chr_ota_control_val,
                                   sizeof(gatt_svr_chr_ota_control_val));
        ble_gatts_notify_custom(conn_handle, ota_control_val_handle, om);
        ESP_LOGI(TAG, "OTA DONE acknowledgement has been sent.");
  
        // restart the ESP to finish the OTA
        if (err == ESP_OK) {
          ESP_LOGI(TAG, "Preparing to restart!");
          gpio_set_level(POWER_LED_GREEN, 1);  // Turn off green LED to notify user ota is done
          gpio_set_level(POWER_LED, 0);
          vTaskDelay(pdMS_TO_TICKS(500));
          esp_restart();
        }
  
        break;
  
      default:
        break;
    }
  }
  

/* Public functions */
void send_gps_data(void) {
    if (gps_data_ind_status && gps_data_chr_conn_handle_inited) {
        ble_gatts_notify(gps_data_chr_conn_handle,gps_data_chr_val_handle);
  
    }
}

/*
 *  Handle GATT attribute register events
 *      - Service register event
 *      - Characteristic register event
 *      - Descriptor register event
 */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    /* Local variables */
    char buf[BLE_UUID_STR_LEN];

    /* Handle GATT attributes register events */
    switch (ctxt->op) {

    /* Service register event */
    case BLE_GATT_REGISTER_OP_SVC:
        ESP_LOGD(TAG, "registered service %s with handle=%d",
                 ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                 ctxt->svc.handle);
        break;

    /* Characteristic register event */
    case BLE_GATT_REGISTER_OP_CHR:
        ESP_LOGD(TAG,
                 "registering characteristic %s with "
                 "def_handle=%d val_handle=%d",
                 ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;

    /* Descriptor register event */
    case BLE_GATT_REGISTER_OP_DSC:
        ESP_LOGD(TAG, "registering descriptor %s with handle=%d",
                 ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                 ctxt->dsc.handle);
        break;

    /* Unknown event */
    default:
        assert(0);
        break;
    }
}

/*
 *  GATT server subscribe event callback
 *      1. Update gps data subscription status
 */

void gatt_svr_subscribe_cb(struct ble_gap_event *event) {
    /* Check connection handle */
    if (event->subscribe.conn_handle != BLE_HS_CONN_HANDLE_NONE) {
        ESP_LOGI(TAG, "subscribe event; conn_handle=%d attr_handle=%d",
                 event->subscribe.conn_handle, event->subscribe.attr_handle);
    } else {
        ESP_LOGI(TAG, "subscribe by nimble stack; attr_handle=%d",
                 event->subscribe.attr_handle);
    }

    /* Check attribute handle */
    if (event->subscribe.attr_handle == gps_data_chr_val_handle) {
        /* Update gps data subscription status */
        gps_data_chr_conn_handle = event->subscribe.conn_handle;
        gps_data_chr_conn_handle_inited = true;
        gps_data_ind_status = event->subscribe.cur_notify;
    }
}

/*
 *  GATT server initialization
 *      1. Initialize GATT service
 *      2. Update NimBLE host GATT services counter
 *      3. Add GATT services to server
 */
int gatt_svc_init(void) {
    /* Local variables */
    int rc;

    /* 1. GATT service initialization */
    ble_svc_gatt_init();

    /* 2. Update GATT services counter */
    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGI(TAG, "Error in count cfg");
        return rc;
    }

    /* 3. Add GATT services */
    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        ESP_LOGI(TAG, "Error in add svcs");
        return rc;
    }


    return 0;
}
