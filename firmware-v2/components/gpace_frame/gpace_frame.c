#include "gpace_frame.h"

#include <string.h>

static void put_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void put_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

void gpace_frame_checksum(const uint8_t *body, size_t len, uint8_t *ck_a, uint8_t *ck_b)
{
    uint8_t a = 0;
    uint8_t b = 0;

    a = (uint8_t)(a + (len & 0xFFu));
    b = (uint8_t)(b + a);
    a = (uint8_t)(a + (len >> 8));
    b = (uint8_t)(b + a);
    for (size_t i = 0; i < len; i++) {
        a = (uint8_t)(a + body[i]);
        b = (uint8_t)(b + a);
    }
    *ck_a = a;
    *ck_b = b;
}

void gpace_frame_from_sensors(const gpace_ubx_pvt_t *pvt, const gpace_bmi270_raw_t *imu, gpace_frame_t *out)
{
    memset(out, 0, sizeof(*out));
    if (pvt != NULL) {
        out->lon_e7 = pvt->lon_e7;
        out->lat_e7 = pvt->lat_e7;
        out->height_mm = pvt->height_mm;
        out->hmsl_mm = pvt->hmsl_mm;
        out->hacc_mm = pvt->hacc_mm;
        out->gspeed_mms = pvt->gspeed_mms;
        out->head_mot_e5 = pvt->head_mot_e5;
        out->sacc_mms = pvt->sacc_mms;
        out->pdop_e2 = pvt->pdop_e2;
        out->num_sv = pvt->num_sv;
        out->second = pvt->second;
        out->minute = pvt->minute;
        out->itow_ms = pvt->itow_ms;
        out->fix_type = pvt->fix_type;
    }
    if (imu != NULL) {
        for (int axis = 0; axis < 3; axis++) {
            out->acc[axis] = imu->acc[axis];
            out->gyr[axis] = imu->gyr[axis];
        }
    }
}

void gpace_frame_encode(const gpace_frame_t *frame, uint8_t out[GPACE_FRAME_SIZE])
{
    out[0] = GPACE_FRAME_SYNC;
    put_u32(&out[1], (uint32_t)frame->lon_e7);
    put_u32(&out[5], (uint32_t)frame->lat_e7);
    put_u32(&out[9], (uint32_t)frame->height_mm);
    put_u32(&out[13], (uint32_t)frame->hmsl_mm);
    put_u32(&out[17], frame->hacc_mm);
    put_u32(&out[21], (uint32_t)frame->gspeed_mms);
    put_u32(&out[25], (uint32_t)frame->head_mot_e5);
    put_u32(&out[29], frame->sacc_mms);
    put_u16(&out[33], frame->pdop_e2);
    out[35] = frame->num_sv;
    out[36] = frame->second;
    out[37] = frame->minute;
    put_u32(&out[38], frame->itow_ms);
    for (int axis = 0; axis < 3; axis++) {
        put_u16(&out[42 + 2 * axis], (uint16_t)frame->acc[axis]);
        put_u16(&out[48 + 2 * axis], (uint16_t)frame->gyr[axis]);
    }
    out[54] = frame->fix_type;
    gpace_frame_checksum(out, GPACE_FRAME_BODY_SIZE, &out[55], &out[56]);
}

gpace_frame_status_t gpace_frame_decode(const uint8_t in[GPACE_FRAME_SIZE], gpace_frame_t *out)
{
    uint8_t ck_a;
    uint8_t ck_b;

    if (in[0] != GPACE_FRAME_SYNC) {
        return GPACE_FRAME_ERR_SYNC;
    }
    gpace_frame_checksum(in, GPACE_FRAME_BODY_SIZE, &ck_a, &ck_b);
    if (ck_a != in[55] || ck_b != in[56]) {
        return GPACE_FRAME_ERR_CHECKSUM;
    }
    memset(out, 0, sizeof(*out));
    out->lon_e7 = (int32_t)get_u32(&in[1]);
    out->lat_e7 = (int32_t)get_u32(&in[5]);
    out->height_mm = (int32_t)get_u32(&in[9]);
    out->hmsl_mm = (int32_t)get_u32(&in[13]);
    out->hacc_mm = get_u32(&in[17]);
    out->gspeed_mms = (int32_t)get_u32(&in[21]);
    out->head_mot_e5 = (int32_t)get_u32(&in[25]);
    out->sacc_mms = get_u32(&in[29]);
    out->pdop_e2 = get_u16(&in[33]);
    out->num_sv = in[35];
    out->second = in[36];
    out->minute = in[37];
    out->itow_ms = get_u32(&in[38]);
    for (int axis = 0; axis < 3; axis++) {
        out->acc[axis] = (int16_t)get_u16(&in[42 + 2 * axis]);
        out->gyr[axis] = (int16_t)get_u16(&in[48 + 2 * axis]);
    }
    out->fix_type = in[54];
    return GPACE_FRAME_OK;
}
