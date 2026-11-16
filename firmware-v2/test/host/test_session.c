#include <string.h>

#include "fake_gnss.h"
#include "gpace_ubx_session.h"
#include "testing.h"

static void test_connect_scans_baud_rates(void)
{
    fake_gnss_t fake;
    gpace_ubx_t ubx;
    const uint32_t bauds[] = { 460800, 38400, 9600 };
    uint32_t found = 0;

    fake_gnss_init(&fake, 9600);
    gpace_ubx_init(&ubx, &fake.link);
    CHECK(gpace_ubx_connect(&ubx, bauds, 3, &found));
    CHECK_EQ(found, 9600);
    CHECK_EQ(fake.host_baud, 9600);
}

static void test_connect_fails_when_no_receiver_answers(void)
{
    fake_gnss_t fake;
    gpace_ubx_t ubx;
    const uint32_t bauds[] = { 460800, 38400 };
    uint32_t found = 123;

    fake_gnss_init(&fake, 115200);
    gpace_ubx_init(&ubx, &fake.link);
    CHECK(!gpace_ubx_connect(&ubx, bauds, 2, &found));
    CHECK_EQ(found, 123);
}

static void test_configuration_is_applied_with_exact_value_sizes(void)
{
    fake_gnss_t fake;
    gpace_ubx_t ubx;
    gpace_ubx_config_t config;
    gpace_ubx_report_t report;
    const uint32_t bauds[] = { 460800, 9600 };
    bool found = false;

    fake_gnss_init(&fake, 9600);
    gpace_ubx_init(&ubx, &fake.link);
    gpace_ubx_config_default(&config);
    CHECK(gpace_ubx_connect(&ubx, bauds, 2, NULL));
    CHECK(gpace_ubx_needs_config(&ubx, &config));
    CHECK(gpace_ubx_apply_config(&ubx, &config, &report));
    CHECK_EQ(report.failed_count, 0);
    CHECK_EQ(report.attempted, report.acknowledged);
    CHECK(report.baud_changed);
    CHECK_EQ(fake.host_baud, 460800);
    CHECK_EQ(fake.device_baud, 460800);

    CHECK_EQ(fake_gnss_value(&fake, GPACE_UBX_KEY_SIGNAL_GPS_ENA, &found), 1);
    CHECK(found);
    CHECK_EQ(fake_gnss_value(&fake, GPACE_UBX_KEY_SIGNAL_GLO_ENA, &found), 1);
    CHECK_EQ(fake_gnss_value(&fake, GPACE_UBX_KEY_NAVSPG_DYNMODEL, &found), 4);
    CHECK_EQ(fake_gnss_value(&fake, GPACE_UBX_KEY_RATE_MEAS, &found), 40);
    CHECK_EQ(fake_gnss_value(&fake, GPACE_UBX_KEY_MSGOUT_NAV_PVT_UART1, &found), 1);
    CHECK_EQ(fake_gnss_value(&fake, GPACE_UBX_KEY_UART1OUTPROT_NMEA, &found), 0);
    CHECK(!gpace_ubx_needs_config(&ubx, &config));
}

static void test_rejected_keys_are_reported_not_hidden(void)
{
    fake_gnss_t fake;
    gpace_ubx_t ubx;
    gpace_ubx_config_t config;
    gpace_ubx_report_t report;

    fake_gnss_init(&fake, 460800);
    fake.host_baud = 460800;
    fake.nak_keys[0] = GPACE_UBX_KEY_SIGNAL_SBAS_ENA;
    fake.nak_count = 1;
    gpace_ubx_init(&ubx, &fake.link);
    gpace_ubx_config_default(&config);
    CHECK(!gpace_ubx_apply_config(&ubx, &config, &report));
    CHECK_EQ(report.failed_count, 1);
    CHECK_EQ(report.failed_keys[0], GPACE_UBX_KEY_SIGNAL_SBAS_ENA);
    CHECK_EQ(report.attempted - report.acknowledged, 1);
}

static void test_automatic_pvt_stream_is_decoded_from_chunked_reads(void)
{
    fake_gnss_t fake;
    gpace_ubx_t ubx;
    gpace_ubx_config_t config;
    gpace_ubx_report_t report;
    gpace_ubx_pvt_t pvt;
    uint32_t last_itow = 0;

    fake_gnss_init(&fake, 460800);
    fake.host_baud = 460800;
    gpace_ubx_init(&ubx, &fake.link);
    gpace_ubx_config_default(&config);
    CHECK(gpace_ubx_apply_config(&ubx, &config, &report));
    fake.read_chunk = 3;
    for (int i = 0; i < 5; i++) {
        CHECK(gpace_ubx_poll(&ubx, &pvt, 200));
        CHECK_EQ(pvt.fix_type, 3);
        CHECK_EQ(pvt.num_sv, 14);
        CHECK_EQ(pvt.lon_e7, 10000000);
        CHECK_EQ(pvt.gspeed_mms, 1000);
        CHECK(pvt.itow_ms > last_itow);
        last_itow = pvt.itow_ms;
    }
    CHECK_EQ(ubx.pvt_count, 5);
    CHECK_EQ(ubx.parser.checksum_errors, 0);
}

static void test_poll_mode_needs_a_request(void)
{
    fake_gnss_t fake;
    gpace_ubx_t ubx;
    gpace_ubx_config_t config;
    gpace_ubx_report_t report;
    gpace_ubx_pvt_t pvt;

    fake_gnss_init(&fake, 460800);
    fake.host_baud = 460800;
    gpace_ubx_init(&ubx, &fake.link);
    gpace_ubx_config_default(&config);
    config.auto_pvt = false;
    CHECK(gpace_ubx_apply_config(&ubx, &config, &report));
    CHECK(!gpace_ubx_poll(&ubx, &pvt, 100));
    CHECK(gpace_ubx_request_pvt(&ubx));
    CHECK(gpace_ubx_poll(&ubx, &pvt, 100));
    CHECK_EQ(pvt.fix_type, 3);
}

void run_session_tests(void)
{
    RUN(test_connect_scans_baud_rates);
    RUN(test_connect_fails_when_no_receiver_answers);
    RUN(test_configuration_is_applied_with_exact_value_sizes);
    RUN(test_rejected_keys_are_reported_not_hidden);
    RUN(test_automatic_pvt_stream_is_decoded_from_chunked_reads);
    RUN(test_poll_mode_needs_a_request);
}
