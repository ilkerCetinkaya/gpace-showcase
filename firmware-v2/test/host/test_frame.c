#include <string.h>

#include "gpace_frame.h"
#include "testing.h"

typedef struct {
    uint8_t longtitude[4];
    uint8_t latitude[4];
    uint8_t altitude[4];
    uint8_t altitude_msl[4];
    uint8_t horizontal_acc[4];
    uint8_t speed[4];
    uint8_t heading[4];
    uint8_t speed_acc[4];
    uint8_t pdop[2];
    uint8_t numSIV;
    uint8_t second;
    uint8_t minute;
    uint8_t fixtype;
    uint8_t millisecond[4];
} v1_result_t;

static void v1_checksum(uint8_t *a, uint8_t *b, const uint8_t *buffer)
{
    *a = 0;
    *b = 0;
    *a += (55 & 0xFF);
    *b += *a;
    *a += (55 >> 8);
    *b += *a;
    for (int i = 0; i < 55; i++) {
        *a += buffer[i];
        *b += *a;
    }
}

static void v1_pack(const v1_result_t *r, const int16_t acc[3], const int16_t gyr[3], uint8_t *buffer)
{
    uint8_t a;
    uint8_t b;
    buffer[0] = 1;
    memcpy(&buffer[1], r->longtitude, 4);
    memcpy(&buffer[5], r->latitude, 4);
    memcpy(&buffer[9], r->altitude, 4);
    memcpy(&buffer[13], r->altitude_msl, 4);
    memcpy(&buffer[17], r->horizontal_acc, 4);
    memcpy(&buffer[21], r->speed, 4);
    memcpy(&buffer[25], r->heading, 4);
    memcpy(&buffer[29], r->speed_acc, 4);
    buffer[33] = r->pdop[0];
    buffer[34] = r->pdop[1];
    buffer[35] = r->numSIV;
    buffer[36] = r->second;
    buffer[37] = r->minute;
    memcpy(&buffer[38], r->millisecond, 4);
    for (int i = 0; i < 3; i++) {
        buffer[42 + 2 * i] = (uint8_t)((uint16_t)acc[i] & 0xFF);
        buffer[43 + 2 * i] = (uint8_t)((uint16_t)acc[i] >> 8);
        buffer[48 + 2 * i] = (uint8_t)((uint16_t)gyr[i] & 0xFF);
        buffer[49 + 2 * i] = (uint8_t)((uint16_t)gyr[i] >> 8);
    }
    buffer[54] = r->fixtype;
    v1_checksum(&a, &b, buffer);
    buffer[55] = a;
    buffer[56] = b;
}

static uint32_t g_seed = 12345;

static uint32_t next_random(void)
{
    g_seed = g_seed * 1664525u + 1013904223u;
    return g_seed >> 8;
}

static void le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static void test_new_encoder_is_byte_identical_to_the_v1_packer(void)
{
    for (int iteration = 0; iteration < 500; iteration++) {
        v1_result_t old = {0};
        gpace_ubx_pvt_t pvt;
        gpace_bmi270_raw_t imu;
        gpace_frame_t frame;
        uint8_t expected[GPACE_FRAME_SIZE];
        uint8_t actual[GPACE_FRAME_SIZE];

        memset(&pvt, 0, sizeof(pvt));
        pvt.lon_e7 = (int32_t)next_random();
        pvt.lat_e7 = (int32_t)next_random();
        pvt.height_mm = (int32_t)next_random();
        pvt.hmsl_mm = (int32_t)next_random();
        pvt.hacc_mm = next_random();
        pvt.gspeed_mms = (int32_t)next_random();
        pvt.head_mot_e5 = (int32_t)next_random();
        pvt.sacc_mms = next_random();
        pvt.pdop_e2 = (uint16_t)next_random();
        pvt.num_sv = (uint8_t)next_random();
        pvt.second = (uint8_t)(next_random() % 60);
        pvt.minute = (uint8_t)(next_random() % 60);
        pvt.itow_ms = next_random();
        pvt.fix_type = (uint8_t)(next_random() % 6);
        for (int axis = 0; axis < 3; axis++) {
            imu.acc[axis] = (int16_t)next_random();
            imu.gyr[axis] = (int16_t)next_random();
        }

        le32(old.longtitude, (uint32_t)pvt.lon_e7);
        le32(old.latitude, (uint32_t)pvt.lat_e7);
        le32(old.altitude, (uint32_t)pvt.height_mm);
        le32(old.altitude_msl, (uint32_t)pvt.hmsl_mm);
        le32(old.horizontal_acc, pvt.hacc_mm);
        le32(old.speed, (uint32_t)pvt.gspeed_mms);
        le32(old.heading, (uint32_t)pvt.head_mot_e5);
        le32(old.speed_acc, pvt.sacc_mms);
        old.pdop[0] = (uint8_t)pvt.pdop_e2;
        old.pdop[1] = (uint8_t)(pvt.pdop_e2 >> 8);
        old.numSIV = pvt.num_sv;
        old.second = pvt.second;
        old.minute = pvt.minute;
        old.fixtype = pvt.fix_type;
        le32(old.millisecond, pvt.itow_ms);

        v1_pack(&old, imu.acc, imu.gyr, expected);
        gpace_frame_from_sensors(&pvt, &imu, &frame);
        gpace_frame_encode(&frame, actual);
        CHECK(memcmp(expected, actual, GPACE_FRAME_SIZE) == 0);
    }
}

static void test_decode_round_trips_and_detects_damage(void)
{
    gpace_frame_t frame;
    gpace_frame_t decoded;
    uint8_t bytes[GPACE_FRAME_SIZE];

    memset(&frame, 0, sizeof(frame));
    frame.lon_e7 = -123456789;
    frame.lat_e7 = 410000000;
    frame.height_mm = -5000;
    frame.hmsl_mm = 120000;
    frame.hacc_mm = 1500;
    frame.gspeed_mms = 83333;
    frame.head_mot_e5 = 27000000;
    frame.sacc_mms = 90;
    frame.pdop_e2 = 120;
    frame.num_sv = 21;
    frame.second = 59;
    frame.minute = 7;
    frame.itow_ms = 432000123u;
    frame.acc[0] = -4096;
    frame.acc[1] = 12;
    frame.acc[2] = 32767;
    frame.gyr[0] = -32768;
    frame.gyr[1] = 1;
    frame.gyr[2] = -1;
    frame.fix_type = 3;
    gpace_frame_encode(&frame, bytes);
    CHECK_EQ(bytes[0], 1);
    CHECK_EQ(gpace_frame_decode(bytes, &decoded), GPACE_FRAME_OK);
    CHECK(memcmp(&frame, &decoded, sizeof(frame)) == 0);

    bytes[30] ^= 0x01;
    CHECK_EQ(gpace_frame_decode(bytes, &decoded), GPACE_FRAME_ERR_CHECKSUM);
    bytes[30] ^= 0x01;
    bytes[0] = 2;
    CHECK_EQ(gpace_frame_decode(bytes, &decoded), GPACE_FRAME_ERR_SYNC);
}

static void test_missing_sensors_produce_a_no_fix_frame(void)
{
    gpace_frame_t frame;
    gpace_bmi270_raw_t imu = { .acc = { 1, 2, 3 }, .gyr = { 4, 5, 6 } };

    gpace_frame_from_sensors(NULL, &imu, &frame);
    CHECK_EQ(frame.fix_type, 0);
    CHECK_EQ(frame.lon_e7, 0);
    CHECK_EQ(frame.acc[2], 3);
    CHECK_EQ(frame.gyr[0], 4);
    gpace_frame_from_sensors(NULL, NULL, &frame);
    CHECK_EQ(frame.acc[0], 0);
}

static void test_checksum_includes_the_length_seed(void)
{
    uint8_t zeros[GPACE_FRAME_BODY_SIZE] = {0};
    uint8_t a;
    uint8_t b;
    gpace_frame_checksum(zeros, sizeof(zeros), &a, &b);
    CHECK_EQ(a, 55);
    CHECK_EQ(b, (110 + 55 * 55) % 256);
}

void run_frame_tests(void)
{
    RUN(test_new_encoder_is_byte_identical_to_the_v1_packer);
    RUN(test_decode_round_trips_and_detects_damage);
    RUN(test_missing_sensors_produce_a_no_fix_frame);
    RUN(test_checksum_includes_the_length_seed);
}
