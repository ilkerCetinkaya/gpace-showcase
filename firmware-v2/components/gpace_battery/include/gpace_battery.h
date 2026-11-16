#ifndef GPACE_BATTERY_H
#define GPACE_BATTERY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPACE_BATTERY_DIVIDER_TOP_OHMS 36000
#define GPACE_BATTERY_DIVIDER_BOTTOM_OHMS 100000
#define GPACE_BATTERY_EMPTY_PIN_MV 2250
#define GPACE_BATTERY_FULL_PIN_MV 3000

int32_t gpace_battery_vbat_mv(int32_t pin_mv);
uint8_t gpace_battery_percent(int32_t pin_mv);

#ifdef __cplusplus
}
#endif

#endif
