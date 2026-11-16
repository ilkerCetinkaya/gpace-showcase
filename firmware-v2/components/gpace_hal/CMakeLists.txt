idf_component_register(
    SRCS "gpace_hal.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_timer esp_rom freertos gpace_board gpace_bmi270 gpace_ubx
)
