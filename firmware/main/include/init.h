#ifndef INIT_H
#define INIT_H

#include "string.h"
#include "ubx_gpace.h"
#include "imu.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"
#include "common.h"

// Pin definitions
#define rx GPIO_NUM_1
#define tx GPIO_NUM_2
#define BUTTON_PIN GPIO_NUM_12 
#define MOSFET_PIN GPIO_NUM_13 
#define STATUS_LED GPIO_NUM_45 // Bluetooth LED
#define POWER_LED GPIO_NUM_38  // Power status LED
#define POWER_LED_RED GPIO_NUM_40  // Shutdown status LED
#define POWER_LED_GREEN GPIO_NUM_39 // OTA status LED
#define GPIO_STATUS_BIT_MASK (1ULL << STATUS_LED)
#define GPIO_BUTTON_BIT_MASK (1ULL << BUTTON_PIN)
#define GPIO_MOSFET_BIT_MASK (1ULL << MOSFET_PIN)
#define GPIO_POWER_LED_BIT_MASK (1ULL << POWER_LED)
#define GPIO_POWER_LED_RED_BIT_MASK (1ULL << POWER_LED_RED)
#define GPIO_POWER_LED_GREEN_BIT_MASK (1ULL << POWER_LED_GREEN)
#define BATT_CHANNEL ADC_CHANNEL_5


// Function declarations
void init(void);
bool adc_get_val(int* result);



#endif