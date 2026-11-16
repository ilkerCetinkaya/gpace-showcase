#include "gpace_battery.h"
#include "testing.h"

void run_battery_tests(void)
{
    CHECK(gpace_battery_percent(2250) == 0);
    CHECK(gpace_battery_percent(3000) == 100);
    CHECK(gpace_battery_percent(2625) == 50);
    CHECK(gpace_battery_percent(0) == 0);
    CHECK(gpace_battery_percent(5000) == 100);
    CHECK(gpace_battery_vbat_mv(3000) == 4080);
    CHECK(gpace_battery_vbat_mv(0) == 0);
}
