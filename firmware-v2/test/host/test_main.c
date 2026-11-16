#include "testing.h"

int g_checks = 0;
int g_failures = 0;

int main(void)
{
    run_ubx_tests();
    run_session_tests();
    run_bmi270_tests();
    run_frame_tests();
    run_battery_tests();
    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
