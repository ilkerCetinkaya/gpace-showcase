#include "gpace_battery.h"

int32_t gpace_battery_vbat_mv(int32_t pin_mv)
{
    return (int32_t)(((int64_t)pin_mv * (GPACE_BATTERY_DIVIDER_TOP_OHMS + GPACE_BATTERY_DIVIDER_BOTTOM_OHMS)) /
                     GPACE_BATTERY_DIVIDER_BOTTOM_OHMS);
}

uint8_t gpace_battery_percent(int32_t pin_mv)
{
    const int32_t span = GPACE_BATTERY_FULL_PIN_MV - GPACE_BATTERY_EMPTY_PIN_MV;
    const int32_t percent = ((pin_mv - GPACE_BATTERY_EMPTY_PIN_MV) * 100) / span;
    if (percent < 0) {
        return 0;
    }
    if (percent > 100) {
        return 100;
    }
    return (uint8_t)percent;
}
