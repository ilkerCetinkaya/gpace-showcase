idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash bt freertos log
             gpace_ble gpace_bmi270 gpace_frame gpace_hal gpace_power gpace_snapshot gpace_ubx
)
