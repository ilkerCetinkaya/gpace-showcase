idf_component_register(
    SRCS "gpace_power.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_adc freertos log gpace_board gpace_battery
)
