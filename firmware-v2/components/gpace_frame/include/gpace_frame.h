#ifndef GPACE_FRAME_H
#define GPACE_FRAME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gpace_bmi270.h"
#include "gpace_ubx.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GPACE_FRAME_SIZE 57u
#define GPACE_FRAME_BODY_SIZE 55u
#define GPACE_FRAME_SYNC 1u

typedef struct {
    int32_t lon_e7;
    int32_t lat_e7;
    int32_t height_mm;
    int32_t hmsl_mm;
    uint32_t hacc_mm;
    int32_t gspeed_mms;
    int32_t head_mot_e5;
    uint32_t sacc_mms;
    uint16_t pdop_e2;
    uint8_t num_sv;
    uint8_t second;
    uint8_t minute;
    uint32_t itow_ms;
    int16_t acc[3];
    int16_t gyr[3];
    uint8_t fix_type;
} gpace_frame_t;

typedef enum {
    GPACE_FRAME_OK = 0,
    GPACE_FRAME_ERR_SYNC = -1,
    GPACE_FRAME_ERR_CHECKSUM = -2
} gpace_frame_status_t;

void gpace_frame_checksum(const uint8_t *body, size_t len, uint8_t *ck_a, uint8_t *ck_b);
void gpace_frame_from_sensors(const gpace_ubx_pvt_t *pvt, const gpace_bmi270_raw_t *imu, gpace_frame_t *out);
void gpace_frame_encode(const gpace_frame_t *frame, uint8_t out[GPACE_FRAME_SIZE]);
gpace_frame_status_t gpace_frame_decode(const uint8_t in[GPACE_FRAME_SIZE], gpace_frame_t *out);

#ifdef __cplusplus
}
#endif

#endif
