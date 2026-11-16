
/* Includes */
#include "common.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2c_master.h"
#include "gap_gpace.h"
#include "gatt_svc_gpace.h"
#include "string.h"
#include "ubx_gpace.h"
#include "imu.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "init.h"

/* Library function declarations */
void ble_store_config_init(void);

/* Private function declarations */
static void on_stack_reset(int reason);
static void on_stack_sync(void);
static void nimble_host_config_init(void);
static void nimble_host_task(void *param);
static void shutdown_task(void* param);

static QueueHandle_t shutdown_evt_queue = NULL;


// Button isr handler to turn off the mosfet in order to power off the device
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint8_t shutdown = 1;
    xQueueSendFromISR(shutdown_evt_queue, &shutdown, NULL);
         
}


static void shutdown_task(void *param)
{
    uint8_t shutdown;
    for (;;)
    {
        if(xQueueReceive(shutdown_evt_queue, &shutdown, portMAX_DELAY))
        {
            if (shutdown == 1)
            {
                gpio_set_level(POWER_LED, 1);
                gpio_set_level(STATUS_LED, 0);
                gpio_set_level(POWER_LED_RED, 0);
                vTaskDelay(600/portTICK_PERIOD_MS);
                gpio_set_level(MOSFET_PIN, 0);      
            }
            
        }
    }
    vTaskDelete(NULL);
}

/* Private functions */
/*
 *  Stack event callback functions
 *      - on_stack_reset is called when host resets BLE stack due to errors
 *      - on_stack_sync is called when host has synced with controller
 */
static void on_stack_reset(int reason) {
    /* On reset, print reset reason to console */
    ESP_LOGI(TAG, "nimble stack reset, reset reason: %d", reason);
}

static void on_stack_sync(void) {
    /* On stack sync, do advertising initialization */
    adv_init();
}

static void nimble_host_config_init(void) {
    /* Set host callbacks */
    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    /* Store host configuration */
    ble_store_config_init();
}

static void nimble_host_task(void *param) {
    /* Task entry log */
    ESP_LOGI(TAG, "nimble host task has been started!");

    /* This function won't return until nimble_port_stop() is executed */
    nimble_port_run();

    /* Clean up at exit */
    vTaskDelete(NULL);
}

static void gps_data_transfer_task(void *param) {
    /* Task entry log */
    ESP_LOGI(TAG, "gps data transfer task has been started!");

    /* Loop forever */
    while (1) {

        /* Send gps data indication if enabled */
        send_gps_data();

        /* Sleep */
        vTaskDelay(10/portTICK_PERIOD_MS);
    }

    /* Clean up at exit */
    vTaskDelete(NULL);
}


void app_main(void) {
    /* Local variables */
    int rc;
    esp_err_t ret;

    /* Init GPIO pins, GPS, IMU and ADC peripherals */
    init();

    /* Initialize ISR for shutdown event */
    shutdown_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, NULL);

    /*
     * NVS flash initialization
     * Dependency of BLE stack to store configurations
     */
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nvs flash, error code: %d ", ret);
        return;
    }

    /* NimBLE stack initialization */
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize nimble stack, error code: %d ",
                 ret);
        return;
    }

    /* GAP service initialization */
    rc = gap_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GAP service, error code: %d", rc);
        return;
    }

    /* GATT server initialization */
    rc = gatt_svc_init();
    if (rc != 0) {
        ESP_LOGE(TAG, "failed to initialize GATT server, error code: %d", rc);
        return;
    }

    /* NimBLE host configuration initialization */
    nimble_host_config_init();


    /* Start NimBLE host task thread and return */
    xTaskCreate(nimble_host_task, "NimBLE Host", 4*1024, NULL, 5, NULL);
    xTaskCreate(gps_data_transfer_task, "GPACE_VT-1", 4*1024, NULL, 5, NULL);
    xTaskCreate(shutdown_task, "shutdown_task",1024, NULL, 6, NULL);
    return;
}
