idf_component_register(
    SRCS "gap_gpace.c" "gatt_svc_gpace.c"
    INCLUDE_DIRS "include"
    REQUIRES bt nvs_flash app_update driver esp_adc freertos log
             gpace_board gpace_battery gpace_frame gpace_power gpace_snapshot
)
