#include <string.h>

#include "gpace_ubx.h"
#include "testing.h"

static void make_pvt_payload(uint8_t *p)
{
    memset(p, 0, GPACE_UBX_PVT_PAYLOAD_LEN);
    p[0] = 0x10; p[1] = 0x27;
    p[4] = 0xEA; p[5] = 0x07;
    p[6] = 11; p[7] = 16; p[8] = 12; p[9] = 34; p[10] = 56;
    p[20] = 3; p[21] = 1; p[23] = 17;
    p[24] = 0x80; p[25] = 0x96; p[26] = 0x98; p[27] = 0x00;
    p[28] = 0x00; p[29] = 0x6C; p[30] = 0xDC; p[31] = 0x02;
    p[32] = 0xE8; p[33] = 0x03;
    p[36] = 0xD0; p[37] = 0x07;
    p[40] = 0x64;
    p[60] = 0xE8; p[61] = 0x03;
    p[64] = 0x10; p[65] = 0x27;
    p[68] = 0x05;
    p[76] = 0x96;
}

static void test_poll_frame_matches_datasheet_example(void)
{
    uint8_t frame[16];
    const uint8_t expected[] = { 0xB5, 0x62, 0x01, 0x07, 0x00, 0x00, 0x08, 0x19 };
    const size_t len = gpace_ubx_build_poll(GPACE_UBX_CLS_NAV, GPACE_UBX_ID_NAV_PVT, frame, sizeof(frame));
    CHECK_EQ(len, sizeof(expected));
    CHECK(memcmp(frame, expected, sizeof(expected)) == 0);
}

static void test_key_sizes_follow_the_key_id(void)
{
    CHECK_EQ(gpace_ubx_key_size(GPACE_UBX_KEY_SIGNAL_GPS_ENA), 1);
    CHECK_EQ(gpace_ubx_key_size(GPACE_UBX_KEY_NAVSPG_DYNMODEL), 1);
    CHECK_EQ(gpace_ubx_key_size(GPACE_UBX_KEY_RATE_MEAS), 2);
    CHECK_EQ(gpace_ubx_key_size(GPACE_UBX_KEY_UART1_BAUDRATE), 4);
    CHECK_EQ(gpace_ubx_key_size(0x00000001u), 0);
}

static void test_valset_uses_exact_value_size(void)
{
    uint8_t frame[32];
    size_t len = gpace_ubx_build_valset(GPACE_UBX_LAYER_RAM, GPACE_UBX_KEY_SIGNAL_GPS_ENA, 1, frame, sizeof(frame));
    CHECK_EQ(len, GPACE_UBX_FRAME_OVERHEAD + 9);
    CHECK_EQ(frame[4], 9);
    CHECK_EQ(frame[7], GPACE_UBX_LAYER_RAM);
    CHECK_EQ(frame[10], 0x1F);
    CHECK_EQ(frame[13], 0x10);
    CHECK_EQ(frame[14], 1);

    len = gpace_ubx_build_valset(GPACE_UBX_LAYER_RAM, GPACE_UBX_KEY_UART1_BAUDRATE, 460800, frame, sizeof(frame));
    CHECK_EQ(len, GPACE_UBX_FRAME_OVERHEAD + 12);
    CHECK_EQ(frame[14], 0x00);
    CHECK_EQ(frame[15], 0x08);
    CHECK_EQ(frame[16], 0x07);
    CHECK_EQ(frame[17], 0x00);

    len = gpace_ubx_build_valset(GPACE_UBX_LAYER_RAM, GPACE_UBX_KEY_RATE_MEAS, 40, frame, sizeof(frame));
    CHECK_EQ(len, GPACE_UBX_FRAME_OVERHEAD + 10);
    CHECK_EQ(gpace_ubx_build_valset(1, 0x00000001u, 1, frame, sizeof(frame)), 0);
    CHECK_EQ(gpace_ubx_build_valset(1, GPACE_UBX_KEY_RATE_MEAS, 40, frame, 4), 0);
}

static size_t feed_all(gpace_ubx_parser_t *parser, const uint8_t *data, size_t len, gpace_ubx_feed_t *last_event)
{
    size_t messages = 0;
    for (size_t i = 0; i < len; i++) {
        const gpace_ubx_feed_t event = gpace_ubx_parser_feed(parser, data[i]);
        if (event == GPACE_UBX_FEED_MESSAGE) {
            messages++;
        }
        if (event != GPACE_UBX_FEED_NONE) {
            *last_event = event;
        }
    }
    return messages;
}

static void test_parser_skips_nmea_noise_and_reads_split_frames(void)
{
    gpace_ubx_parser_t parser;
    uint8_t stream[200];
    uint8_t payload[GPACE_UBX_PVT_PAYLOAD_LEN];
    const char *noise = "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    size_t used = strlen(noise);
    gpace_ubx_feed_t event = GPACE_UBX_FEED_NONE;

    gpace_ubx_parser_reset(&parser);
    memcpy(stream, noise, used);
    make_pvt_payload(payload);
    used += gpace_ubx_encode(GPACE_UBX_CLS_NAV, GPACE_UBX_ID_NAV_PVT, payload, sizeof(payload), &stream[used], sizeof(stream) - used);

    CHECK_EQ(feed_all(&parser, stream, used, &event), 1);
    CHECK_EQ(parser.cls, GPACE_UBX_CLS_NAV);
    CHECK_EQ(parser.id, GPACE_UBX_ID_NAV_PVT);
    CHECK_EQ(parser.len, GPACE_UBX_PVT_PAYLOAD_LEN);
    CHECK_EQ(parser.messages, 1);
}

static void test_parser_reports_checksum_errors_and_recovers(void)
{
    gpace_ubx_parser_t parser;
    uint8_t frame[256];
    uint8_t payload[GPACE_UBX_PVT_PAYLOAD_LEN];
    size_t used;
    gpace_ubx_feed_t event = GPACE_UBX_FEED_NONE;

    gpace_ubx_parser_reset(&parser);
    make_pvt_payload(payload);
    used = gpace_ubx_encode(GPACE_UBX_CLS_NAV, GPACE_UBX_ID_NAV_PVT, payload, sizeof(payload), frame, sizeof(frame));
    frame[10] ^= 0x55;
    CHECK_EQ(feed_all(&parser, frame, used, &event), 0);
    CHECK_EQ(event, GPACE_UBX_FEED_BAD_CHECKSUM);
    CHECK_EQ(parser.checksum_errors, 1);
    frame[10] ^= 0x55;
    CHECK_EQ(feed_all(&parser, frame, used, &event), 1);
    CHECK_EQ(gpace_ubx_encode(GPACE_UBX_CLS_NAV, GPACE_UBX_ID_NAV_PVT, payload, sizeof(payload), frame, 64), 0);
}

static void test_parser_rejects_oversize_frames(void)
{
    gpace_ubx_parser_t parser;
    const uint8_t header[] = { 0xB5, 0x62, 0x0A, 0x04, 0xFF, 0x01 };
    gpace_ubx_feed_t event = GPACE_UBX_FEED_NONE;

    gpace_ubx_parser_reset(&parser);
    CHECK_EQ(feed_all(&parser, header, sizeof(header), &event), 0);
    CHECK_EQ(event, GPACE_UBX_FEED_OVERSIZE);
    CHECK_EQ(parser.oversize, 1);
}

static void test_nav_pvt_decoding(void)
{
    uint8_t payload[GPACE_UBX_PVT_PAYLOAD_LEN];
    gpace_ubx_pvt_t pvt;
    make_pvt_payload(payload);
    CHECK(gpace_ubx_decode_nav_pvt(payload, sizeof(payload), &pvt));
    CHECK_EQ(pvt.itow_ms, 10000);
    CHECK_EQ(pvt.year, 2026);
    CHECK_EQ(pvt.minute, 34);
    CHECK_EQ(pvt.second, 56);
    CHECK_EQ(pvt.fix_type, 3);
    CHECK_EQ(pvt.num_sv, 17);
    CHECK_EQ(pvt.lon_e7, 10000000);
    CHECK_EQ(pvt.lat_e7, 48000000);
    CHECK_EQ(pvt.height_mm, 1000);
    CHECK_EQ(pvt.hmsl_mm, 2000);
    CHECK_EQ(pvt.hacc_mm, 100);
    CHECK_EQ(pvt.gspeed_mms, 1000);
    CHECK_EQ(pvt.head_mot_e5, 10000);
    CHECK_EQ(pvt.sacc_mms, 5);
    CHECK_EQ(pvt.pdop_e2, 150);
    CHECK(!gpace_ubx_decode_nav_pvt(payload, GPACE_UBX_PVT_PAYLOAD_LEN - 1, &pvt));
}

static void test_ack_and_valget_decoding(void)
{
    gpace_ubx_parser_t parser;
    uint8_t frame[32];
    const uint8_t ack_payload[] = { GPACE_UBX_CLS_CFG, GPACE_UBX_ID_CFG_VALSET };
    uint8_t valget_payload[10] = { 0, 0, 0, 0, 0x01, 0x00, 0x21, 0x30, 40, 0 };
    gpace_ubx_feed_t event = GPACE_UBX_FEED_NONE;
    uint8_t cls = 0;
    uint8_t id = 0;
    bool is_ack = false;
    uint64_t value = 0;
    size_t len;

    gpace_ubx_parser_reset(&parser);
    len = gpace_ubx_encode(GPACE_UBX_CLS_ACK, GPACE_UBX_ID_ACK_ACK, ack_payload, 2, frame, sizeof(frame));
    CHECK_EQ(feed_all(&parser, frame, len, &event), 1);
    CHECK(gpace_ubx_decode_ack(&parser, &cls, &id, &is_ack));
    CHECK(is_ack);
    CHECK_EQ(cls, GPACE_UBX_CLS_CFG);
    CHECK_EQ(id, GPACE_UBX_ID_CFG_VALSET);

    len = gpace_ubx_encode(GPACE_UBX_CLS_ACK, GPACE_UBX_ID_ACK_NAK, ack_payload, 2, frame, sizeof(frame));
    CHECK_EQ(feed_all(&parser, frame, len, &event), 1);
    CHECK(gpace_ubx_decode_ack(&parser, &cls, &id, &is_ack));
    CHECK(!is_ack);

    len = gpace_ubx_encode(GPACE_UBX_CLS_CFG, GPACE_UBX_ID_CFG_VALGET, valget_payload, sizeof(valget_payload), frame, sizeof(frame));
    CHECK_EQ(feed_all(&parser, frame, len, &event), 1);
    CHECK(gpace_ubx_decode_valget(&parser, GPACE_UBX_KEY_RATE_MEAS, &value));
    CHECK_EQ(value, 40);
    CHECK(!gpace_ubx_decode_valget(&parser, GPACE_UBX_KEY_RATE_NAV, &value));
}

void run_ubx_tests(void)
{
    RUN(test_poll_frame_matches_datasheet_example);
    RUN(test_key_sizes_follow_the_key_id);
    RUN(test_valset_uses_exact_value_size);
    RUN(test_parser_skips_nmea_noise_and_reads_split_frames);
    RUN(test_parser_reports_checksum_errors_and_recovers);
    RUN(test_parser_rejects_oversize_frames);
    RUN(test_nav_pvt_decoding);
    RUN(test_ack_and_valget_decoding);
}
