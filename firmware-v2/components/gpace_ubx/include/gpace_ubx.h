#ifndef GPACE_UBX_H
#define GPACE_UBX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GPACE_UBX_SYNC_1 0xB5u
#define GPACE_UBX_SYNC_2 0x62u
#define GPACE_UBX_MAX_PAYLOAD 128u
#define GPACE_UBX_FRAME_OVERHEAD 8u
#define GPACE_UBX_PVT_PAYLOAD_LEN 92u

#define GPACE_UBX_CLS_NAV 0x01u
#define GPACE_UBX_CLS_ACK 0x05u
#define GPACE_UBX_CLS_CFG 0x06u
#define GPACE_UBX_ID_NAV_PVT 0x07u
#define GPACE_UBX_ID_ACK_NAK 0x00u
#define GPACE_UBX_ID_ACK_ACK 0x01u
#define GPACE_UBX_ID_CFG_VALSET 0x8Au
#define GPACE_UBX_ID_CFG_VALGET 0x8Bu

#define GPACE_UBX_LAYER_RAM 0x01u
#define GPACE_UBX_LAYER_BBR 0x02u
#define GPACE_UBX_VALGET_LAYER_RAM 0x00u

#define GPACE_UBX_KEY_SIGNAL_GPS_ENA 0x1031001Fu
#define GPACE_UBX_KEY_SIGNAL_SBAS_ENA 0x10310020u
#define GPACE_UBX_KEY_SIGNAL_GAL_ENA 0x10310021u
#define GPACE_UBX_KEY_SIGNAL_BDS_ENA 0x10310022u
#define GPACE_UBX_KEY_SIGNAL_QZSS_ENA 0x10310024u
#define GPACE_UBX_KEY_SIGNAL_GLO_ENA 0x10310025u
#define GPACE_UBX_KEY_NAVSPG_DYNMODEL 0x20110021u
#define GPACE_UBX_KEY_RATE_MEAS 0x30210001u
#define GPACE_UBX_KEY_RATE_NAV 0x30210002u
#define GPACE_UBX_KEY_UART1_BAUDRATE 0x40520001u
#define GPACE_UBX_KEY_UART1INPROT_UBX 0x10730001u
#define GPACE_UBX_KEY_UART1OUTPROT_UBX 0x10740001u
#define GPACE_UBX_KEY_UART1OUTPROT_NMEA 0x10740002u
#define GPACE_UBX_KEY_MSGOUT_NAV_PVT_UART1 0x20910007u

#define GPACE_UBX_DYNMODEL_AUTOMOTIVE 4u

typedef struct {
    uint32_t itow_ms;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t valid;
    uint8_t fix_type;
    uint8_t flags;
    uint8_t num_sv;
    int32_t lon_e7;
    int32_t lat_e7;
    int32_t height_mm;
    int32_t hmsl_mm;
    uint32_t hacc_mm;
    uint32_t vacc_mm;
    int32_t vel_n_mms;
    int32_t vel_e_mms;
    int32_t vel_d_mms;
    int32_t gspeed_mms;
    int32_t head_mot_e5;
    uint32_t sacc_mms;
    uint32_t head_acc_e5;
    uint16_t pdop_e2;
} gpace_ubx_pvt_t;

typedef enum {
    GPACE_UBX_FEED_NONE = 0,
    GPACE_UBX_FEED_MESSAGE,
    GPACE_UBX_FEED_BAD_CHECKSUM,
    GPACE_UBX_FEED_OVERSIZE
} gpace_ubx_feed_t;

typedef struct {
    uint8_t state;
    uint8_t cls;
    uint8_t id;
    uint16_t len;
    uint16_t index;
    uint8_t ck_a;
    uint8_t ck_b;
    uint8_t payload[GPACE_UBX_MAX_PAYLOAD];
    uint32_t messages;
    uint32_t checksum_errors;
    uint32_t oversize;
} gpace_ubx_parser_t;

void gpace_ubx_checksum(const uint8_t *data, size_t len, uint8_t *ck_a, uint8_t *ck_b);
size_t gpace_ubx_encode(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len, uint8_t *out, size_t capacity);

size_t gpace_ubx_key_size(uint32_t key);
size_t gpace_ubx_build_valset(uint8_t layers, uint32_t key, uint64_t value, uint8_t *out, size_t capacity);
size_t gpace_ubx_build_valget(uint8_t layer, uint32_t key, uint8_t *out, size_t capacity);
size_t gpace_ubx_build_poll(uint8_t cls, uint8_t id, uint8_t *out, size_t capacity);

void gpace_ubx_parser_reset(gpace_ubx_parser_t *parser);
gpace_ubx_feed_t gpace_ubx_parser_feed(gpace_ubx_parser_t *parser, uint8_t byte);

bool gpace_ubx_decode_ack(const gpace_ubx_parser_t *msg, uint8_t *acked_cls, uint8_t *acked_id, bool *is_ack);
bool gpace_ubx_decode_valget(const gpace_ubx_parser_t *msg, uint32_t expected_key, uint64_t *value);
bool gpace_ubx_decode_nav_pvt(const uint8_t *payload, uint16_t len, gpace_ubx_pvt_t *out);

#ifdef __cplusplus
}
#endif

#endif
