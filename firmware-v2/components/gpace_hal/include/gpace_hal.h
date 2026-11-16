#ifndef GPACE_HAL_H
#define GPACE_HAL_H

#include "esp_err.h"
#include "gpace_bmi270.h"
#include "gpace_ubx_session.h"

esp_err_t gpace_hal_gnss_link_open(gpace_ubx_link_t *link);
esp_err_t gpace_hal_imu_bus_open(gpace_bmi270_bus_t *bus);

#endif
