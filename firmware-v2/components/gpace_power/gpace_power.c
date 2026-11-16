#include "gpace_power.h"

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "gpace_battery.h"
#include "gpace_board.h"

#define SHUTDOWN_DELAY_MS 600
#define BATTERY_SAMPLES 8

static const char *TAG = "gpace_power";
static QueueHandle_t shutdown_events;
static adc_oneshot_unit_handle_t adc_unit;
static adc_cali_handle_t adc_cali;
static bool adc_calibrated;

static void IRAM_ATTR button_isr(void *arg)
{
    const uint8_t event = 1;
    BaseType_t woken = pdFALSE;
    (void)arg;
    xQueueSendFromISR(shutdown_events, &event, &woken);
    if (woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void shutdown_task(void *param)
{
    uint8_t event;
    (void)param;
    for (;;) {
        if (xQueueReceive(shutdown_events, &event, portMAX_DELAY) == pdTRUE) {
            gpio_set_level(GPACE_PIN_LED_POWER, 1);
            gpio_set_level(GPACE_PIN_LED_BLUETOOTH, 0);
            gpio_set_level(GPACE_PIN_LED_FAULT, 0);
            vTaskDelay(pdMS_TO_TICKS(SHUTDOWN_DELAY_MS));
            gpio_set_level(GPACE_PIN_POWER_LATCH, 0);
        }
    }
}

static esp_err_t configure_outputs(void)
{
    const gpio_config_t outputs = {
        .pin_bit_mask = (1ULL << GPACE_PIN_POWER_LATCH) | (1ULL << GPACE_PIN_LED_BLUETOOTH) |
                        (1ULL << GPACE_PIN_LED_POWER) | (1ULL << GPACE_PIN_LED_OTA) | (1ULL << GPACE_PIN_LED_FAULT),
        .mode = GPIO_MODE_OUTPUT,
    };
    esp_err_t err = gpio_config(&outputs);
    if (err != ESP_OK) {
        return err;
    }
    gpio_set_level(GPACE_PIN_POWER_LATCH, 1);
    gpio_set_level(GPACE_PIN_LED_POWER, 0);
    gpio_set_level(GPACE_PIN_LED_OTA, 1);
    gpio_set_level(GPACE_PIN_LED_FAULT, 1);
    gpio_set_level(GPACE_PIN_LED_BLUETOOTH, 0);
    return ESP_OK;
}

static esp_err_t configure_button(void)
{
    const gpio_config_t button = {
        .pin_bit_mask = 1ULL << GPACE_PIN_BUTTON,
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    esp_err_t err = gpio_config(&button);
    if (err != ESP_OK) {
        return err;
    }
    shutdown_events = xQueueCreate(4, sizeof(uint8_t));
    if (shutdown_events == NULL) {
        return ESP_ERR_NO_MEM;
    }
    err = gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = gpio_isr_handler_add(GPACE_PIN_BUTTON, button_isr, NULL);
    if (err != ESP_OK) {
        return err;
    }
    return xTaskCreate(shutdown_task, "gpace_shutdown", 2048, NULL, 6, NULL) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t configure_adc(void)
{
    const adc_oneshot_unit_init_cfg_t unit_config = {.unit_id = GPACE_ADC_BATTERY_UNIT};
    const adc_oneshot_chan_cfg_t channel_config = {.atten = ADC_ATTEN_DB_12, .bitwidth = ADC_BITWIDTH_DEFAULT};
    const adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = GPACE_ADC_BATTERY_UNIT,
        .chan = GPACE_ADC_BATTERY_CHANNEL,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    esp_err_t err = adc_oneshot_new_unit(&unit_config, &adc_unit);
    if (err != ESP_OK) {
        return err;
    }
    err = adc_oneshot_config_channel(adc_unit, GPACE_ADC_BATTERY_CHANNEL, &channel_config);
    if (err != ESP_OK) {
        return err;
    }
    adc_calibrated = adc_cali_create_scheme_curve_fitting(&cali_config, &adc_cali) == ESP_OK;
    if (!adc_calibrated) {
        ESP_LOGW(TAG, "ADC calibration unavailable, using raw scaling");
    }
    return ESP_OK;
}

esp_err_t gpace_power_init(void)
{
    esp_err_t err = configure_outputs();
    if (err != ESP_OK) {
        return err;
    }
    err = configure_button();
    if (err != ESP_OK) {
        return err;
    }
    return configure_adc();
}

uint8_t gpace_power_battery_percent(void)
{
    int64_t total_mv = 0;
    int taken = 0;
    for (int i = 0; i < BATTERY_SAMPLES; i++) {
        int raw = 0;
        int mv = 0;
        if (adc_oneshot_read(adc_unit, GPACE_ADC_BATTERY_CHANNEL, &raw) != ESP_OK) {
            continue;
        }
        if (adc_calibrated) {
            if (adc_cali_raw_to_voltage(adc_cali, raw, &mv) != ESP_OK) {
                continue;
            }
        } else {
            mv = raw * 3100 / 4095;
        }
        total_mv += mv;
        taken++;
    }
    if (taken == 0) {
        return 0;
    }
    return gpace_battery_percent((int32_t)(total_mv / taken));
}

void gpace_power_indicate_fault(void)
{
    gpio_set_level(GPACE_PIN_LED_FAULT, 0);
}
