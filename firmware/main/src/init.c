#include "init.h"

// ADC global variables

static bool do_calibration2 = false;
static adc_oneshot_unit_handle_t adc2_handle = NULL;
static adc_cali_handle_t adc2_cali_handle = NULL;

// ADC calibration function
static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle);

// Error led declaration
static void error_led_task(void* params);


static bool adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

    if (!calibrated) {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle); // Curve fitting used to enhance the resolution of the battery
        if (ret == ESP_OK) {
            calibrated = true;
        }
    }

    *out_handle = handle;
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Calibration Success");
    } else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated) {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    } else {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

bool adc_get_val(int* result)
{
    /* This function will poll 1000 adc values, take average and write it to result */
    static int adc_raw = 0;
    static int voltage = 0;

    static int average = 0;

    for (int i = 0; i < 1000; i++)
    {
        ESP_ERROR_CHECK_WITHOUT_ABORT(adc_oneshot_read(adc2_handle, BATT_CHANNEL, &adc_raw));
    
        if (do_calibration2)
        {
            ESP_ERROR_CHECK_WITHOUT_ABORT(adc_cali_raw_to_voltage(adc2_cali_handle, adc_raw, &voltage));
        }
        average += voltage;
    }
    
    average = average / 1000;
    result[0] = average;

    average = 0;

    return true;
}

static void error_led_task(void* params)
{
    while (1)
    {
        gpio_set_level(POWER_LED, 1);
        gpio_set_level(STATUS_LED, 0);
        gpio_set_level(POWER_LED_RED, 0);
        vTaskDelay(300/portTICK_PERIOD_MS);
        gpio_set_level(POWER_LED_RED, 1);
        vTaskDelay(400/portTICK_PERIOD_MS);
        gpio_set_level(POWER_LED_RED, 0);
        vTaskDelay(1000/portTICK_PERIOD_MS);
        
    }
    
    vTaskDelete(NULL);
}

void init(void) {

    /* This function is to configure gpio pins (pin_mux) initialize ADC,
    GPS and IMU */

    /* Local variables */
    int ret_gps = 0;
    int ret_imu = 0;

    gpio_config_t config_status;
    
    config_status.mode = GPIO_MODE_OUTPUT;
    config_status.intr_type = GPIO_INTR_DISABLE;
    config_status.pin_bit_mask = GPIO_STATUS_BIT_MASK;
    config_status.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config_status.pull_up_en = GPIO_PULLUP_DISABLE;

    gpio_config(&config_status);

    gpio_config_t config_button;
    
    config_button.mode = GPIO_MODE_INPUT;
    config_button.intr_type = GPIO_INTR_POSEDGE;
    config_button.pin_bit_mask = GPIO_BUTTON_BIT_MASK;
    config_button.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config_button.pull_up_en = GPIO_PULLUP_ENABLE; // Pull Up enabled for button

    gpio_config(&config_button);
   

    gpio_config_t config_mosfet;
    
    config_mosfet.mode = GPIO_MODE_OUTPUT;
    config_mosfet.intr_type = GPIO_INTR_DISABLE;
    config_mosfet.pin_bit_mask = GPIO_MOSFET_BIT_MASK;
    config_mosfet.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config_mosfet.pull_up_en = GPIO_PULLUP_DISABLE; 

    gpio_config(&config_mosfet);

    gpio_config_t config_power_led;
    
    config_power_led.mode = GPIO_MODE_OUTPUT;
    config_power_led.intr_type = GPIO_INTR_DISABLE;
    config_power_led.pin_bit_mask = GPIO_POWER_LED_BIT_MASK;
    config_power_led.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config_power_led.pull_up_en = GPIO_PULLUP_ENABLE; // Pull up enabled for power led

    gpio_config(&config_power_led);

    gpio_config_t config_power_led_red;
    
    config_power_led_red.mode = GPIO_MODE_OUTPUT;
    config_power_led_red.intr_type = GPIO_INTR_DISABLE;
    config_power_led_red.pin_bit_mask = GPIO_POWER_LED_RED_BIT_MASK;
    config_power_led_red.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config_power_led_red.pull_up_en = GPIO_PULLUP_ENABLE; // Pull up enabled for power led

    gpio_config(&config_power_led_red);

    gpio_config_t config_power_led_green;
    
    config_power_led_green.mode = GPIO_MODE_OUTPUT;
    config_power_led_green.intr_type = GPIO_INTR_DISABLE;
    config_power_led_green.pin_bit_mask = GPIO_POWER_LED_GREEN_BIT_MASK;
    config_power_led_green.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config_power_led_green.pull_up_en = GPIO_PULLUP_ENABLE; // Pull up enabled for power led

    gpio_config(&config_power_led_green);

    gpio_set_level(MOSFET_PIN, 1);
    gpio_set_level(STATUS_LED, 0);
    gpio_set_level(POWER_LED, 0);
    gpio_set_level(POWER_LED_RED, 1);
    gpio_set_level(POWER_LED_GREEN, 1);


    vTaskDelay(500/portTICK_PERIOD_MS); // Delay added in order to prevent errors installing communication drivers on power up

    // IMU initialization
    ret_imu = init_imu();

    if (ret_imu != 1)
    {
        ESP_LOGI("INIT","IMU initialization failed.");

    }
    else
    {
        ESP_LOGI("INIT","IMU Status: %d", ret_imu);

    }
    
    
    for (int i = 0; i < 3; i++)
    {
        // GPS initialization
        ret_gps = ubx_connect(tx, rx);
       

        if (ret_gps == 0)
        {
            ESP_LOGI("INIT","ubx connect failed. Retrying attempt.");
            ret_gps = ubx_connect(tx,rx);
        }
        else
        {
            ESP_LOGI("INIT", "GPS connected successfully");
            break;
        }
    }

    if ((ret_gps != 1) || (ret_imu != 1))
    {
        /* If IMU and GPS initialization failed, device will restart itself and inform the user that it failed*/

        ESP_LOGI("INIT","Error initializing GPS or IMU. Restarting. If it is not solved, contact Gpace Tech. service");
        xTaskCreate(error_led_task, "LED Task", 128, NULL, 3, NULL);
         
    }
    


    
    // ADC configuration
    adc_oneshot_unit_init_cfg_t init_config2 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
     adc_oneshot_chan_cfg_t config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc_oneshot_new_unit(&init_config2, &adc2_handle));

    //-------------ADC2 Calibration Init---------------//
    do_calibration2 = adc_calibration_init(ADC_UNIT_1, BATT_CHANNEL, ADC_ATTEN_DB_12, &adc2_cali_handle);

    //-------------ADC2 Config---------------//
    ESP_ERROR_CHECK_WITHOUT_ABORT(adc_oneshot_config_channel(adc2_handle, BATT_CHANNEL, &config));

   
}
