#include <string.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gap_gpace.h"
#include "gatt_svc_gpace.h"
#include "gpace_bmi270.h"
#include "gpace_frame.h"
#include "gpace_hal.h"
#include "gpace_power.h"
#include "gpace_snapshot.h"
#include "gpace_ubx_session.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"

#define SENSOR_TASK_STACK 6144
#define NOTIFY_TASK_STACK 3072
#define NOTIFY_PERIOD_MS 10
#define PVT_WAIT_MS 100
#define MAX_CONSECUTIVE_SILENT_POLLS 50
#define STARTUP_RETRY_LIMIT 5
#define STARTUP_RETRY_DELAY_MS 500

static const char *TAG = "gpace_main";

void ble_store_config_init(void);

static void on_stack_reset(int reason)
{
    ESP_LOGW(TAG, "nimble reset, reason %d", reason);
}

static void on_stack_sync(void)
{
    adv_init();
}

static void nimble_host_task(void *param)
{
    (void)param;
    nimble_port_run();
    vTaskDelete(NULL);
}

static void notify_task(void *param)
{
    (void)param;
    for (;;) {
        send_gps_data();
        vTaskDelay(pdMS_TO_TICKS(NOTIFY_PERIOD_MS));
    }
}

static bool start_imu(gpace_bmi270_t *imu)
{
    gpace_bmi270_bus_t bus;
    gpace_bmi270_config_t config;
    gpace_bmi270_config_default(&config);
    for (int attempt = 0; attempt < STARTUP_RETRY_LIMIT; attempt++) {
        if (gpace_hal_imu_bus_open(&bus) == ESP_OK && gpace_bmi270_init(imu, &bus, &config) == GPACE_BMI270_OK) {
            return true;
        }
        ESP_LOGW(TAG, "imu init attempt %d failed", attempt + 1);
        vTaskDelay(pdMS_TO_TICKS(STARTUP_RETRY_DELAY_MS));
    }
    return false;
}

static bool start_gnss(gpace_ubx_t *ubx, gpace_ubx_link_t *link)
{
    static const uint32_t bauds[] = {460800, 115200, 38400, 9600};
    gpace_ubx_config_t config;
    gpace_ubx_report_t report;
    uint32_t found = 0;
    gpace_ubx_config_default(&config);
    if (gpace_hal_gnss_link_open(link) != ESP_OK) {
        return false;
    }
    gpace_ubx_init(ubx, link);
    for (int attempt = 0; attempt < STARTUP_RETRY_LIMIT; attempt++) {
        if (gpace_ubx_connect(ubx, bauds, sizeof(bauds) / sizeof(bauds[0]), &found)) {
            ESP_LOGI(TAG, "gnss answered at %u baud", (unsigned)found);
            if (!gpace_ubx_needs_config(ubx, &config)) {
                return true;
            }
            memset(&report, 0, sizeof(report));
            if (gpace_ubx_apply_config(ubx, &config, &report)) {
                ESP_LOGI(TAG, "gnss configured, %u/%u keys acknowledged", report.acknowledged, report.attempted);
                return true;
            }
            ESP_LOGW(TAG, "gnss config incomplete, %u keys failed", report.failed_count);
        }
        vTaskDelay(pdMS_TO_TICKS(STARTUP_RETRY_DELAY_MS));
    }
    return false;
}

static void sensor_task(void *param)
{
    gpace_ubx_link_t link;
    gpace_ubx_t ubx;
    gpace_bmi270_t imu;
    gpace_ubx_pvt_t pvt;
    gpace_bmi270_raw_t raw;
    gpace_frame_t frame;
    uint8_t wire[GPACE_FRAME_SIZE];
    int silent_polls = 0;
    (void)param;

    if (!start_imu(&imu)) {
        ESP_LOGE(TAG, "imu unavailable, restarting");
        gpace_power_indicate_fault();
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    if (!start_gnss(&ubx, &link)) {
        ESP_LOGE(TAG, "gnss unavailable, restarting");
        gpace_power_indicate_fault();
        vTaskDelay(pdMS_TO_TICKS(1000));
        esp_restart();
    }
    memset(&pvt, 0, sizeof(pvt));
    memset(&raw, 0, sizeof(raw));

    for (;;) {
        if (gpace_ubx_poll(&ubx, &pvt, PVT_WAIT_MS)) {
            silent_polls = 0;
            if (gpace_bmi270_read_raw(&imu, &raw) != GPACE_BMI270_OK) {
                ESP_LOGW(TAG, "imu read failed");
            }
            gpace_frame_from_sensors(&pvt, &raw, &frame);
            gpace_frame_encode(&frame, wire);
            gpace_snapshot_publish(wire);
        } else {
            silent_polls++;
            gpace_ubx_request_pvt(&ubx);
            if (silent_polls >= MAX_CONSECUTIVE_SILENT_POLLS) {
                ESP_LOGE(TAG, "gnss silent, restarting");
                gpace_power_indicate_fault();
                esp_restart();
            }
        }
    }
}

void app_main(void)
{
    esp_err_t err;

    if (gpace_power_init() != ESP_OK) {
        ESP_LOGE(TAG, "power init failed");
        return;
    }
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_ERROR_CHECK(nimble_port_init());
    ESP_ERROR_CHECK(gap_init());
    ESP_ERROR_CHECK(gatt_svc_init());

    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_store_config_init();

    xTaskCreate(nimble_host_task, "nimble_host", 4096, NULL, 5, NULL);
    xTaskCreate(sensor_task, "gpace_sensor", SENSOR_TASK_STACK, NULL, 5, NULL);
    xTaskCreate(notify_task, "gpace_notify", NOTIFY_TASK_STACK, NULL, 4, NULL);
}
