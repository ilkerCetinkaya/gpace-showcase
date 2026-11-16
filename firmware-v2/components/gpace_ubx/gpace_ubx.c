#include "gpace_ubx.h"

#include <string.h>

enum {
    STATE_SYNC_1 = 0,
    STATE_SYNC_2,
    STATE_CLASS,
    STATE_ID,
    STATE_LEN_LO,
    STATE_LEN_HI,
    STATE_PAYLOAD,
    STATE_CK_A,
    STATE_CK_B
};

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t read_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int32_t read_i32(const uint8_t *p)
{
    return (int32_t)read_u32(p);
}

static void write_u32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void fletcher_step(uint8_t *ck_a, uint8_t *ck_b, uint8_t byte)
{
    *ck_a = (uint8_t)(*ck_a + byte);
    *ck_b = (uint8_t)(*ck_b + *ck_a);
}

void gpace_ubx_checksum(const uint8_t *data, size_t len, uint8_t *ck_a, uint8_t *ck_b)
{
    uint8_t a = 0;
    uint8_t b = 0;
    for (size_t i = 0; i < len; i++) {
        fletcher_step(&a, &b, data[i]);
    }
    *ck_a = a;
    *ck_b = b;
}

size_t gpace_ubx_encode(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len, uint8_t *out, size_t capacity)
{
    const size_t total = (size_t)len + GPACE_UBX_FRAME_OVERHEAD;
    if (capacity < total || (len > 0 && payload == NULL)) {
        return 0;
    }
    out[0] = GPACE_UBX_SYNC_1;
    out[1] = GPACE_UBX_SYNC_2;
    out[2] = cls;
    out[3] = id;
    out[4] = (uint8_t)(len & 0xFFu);
    out[5] = (uint8_t)(len >> 8);
    if (len > 0) {
        memcpy(&out[6], payload, len);
    }
    gpace_ubx_checksum(&out[2], (size_t)len + 4u, &out[6 + len], &out[7 + len]);
    return total;
}

size_t gpace_ubx_key_size(uint32_t key)
{
    switch ((key >> 28) & 0x7u) {
    case 1:
    case 2:
        return 1;
    case 3:
        return 2;
    case 4:
        return 4;
    case 5:
        return 8;
    default:
        return 0;
    }
}

size_t gpace_ubx_build_valset(uint8_t layers, uint32_t key, uint64_t value, uint8_t *out, size_t capacity)
{
    const size_t value_size = gpace_ubx_key_size(key);
    uint8_t payload[4 + 4 + 8];
    if (value_size == 0) {
        return 0;
    }
    memset(payload, 0, sizeof(payload));
    payload[0] = 0;
    payload[1] = layers;
    write_u32(&payload[4], key);
    for (size_t i = 0; i < value_size; i++) {
        payload[8 + i] = (uint8_t)(value >> (8u * i));
    }
    return gpace_ubx_encode(GPACE_UBX_CLS_CFG, GPACE_UBX_ID_CFG_VALSET, payload, (uint16_t)(8 + value_size), out, capacity);
}

size_t gpace_ubx_build_valget(uint8_t layer, uint32_t key, uint8_t *out, size_t capacity)
{
    uint8_t payload[8] = {0};
    if (gpace_ubx_key_size(key) == 0) {
        return 0;
    }
    payload[1] = layer;
    write_u32(&payload[4], key);
    return gpace_ubx_encode(GPACE_UBX_CLS_CFG, GPACE_UBX_ID_CFG_VALGET, payload, sizeof(payload), out, capacity);
}

size_t gpace_ubx_build_poll(uint8_t cls, uint8_t id, uint8_t *out, size_t capacity)
{
    return gpace_ubx_encode(cls, id, NULL, 0, out, capacity);
}

void gpace_ubx_parser_reset(gpace_ubx_parser_t *parser)
{
    memset(parser, 0, sizeof(*parser));
}

gpace_ubx_feed_t gpace_ubx_parser_feed(gpace_ubx_parser_t *parser, uint8_t byte)
{
    switch (parser->state) {
    case STATE_SYNC_1:
        if (byte == GPACE_UBX_SYNC_1) {
            parser->state = STATE_SYNC_2;
        }
        return GPACE_UBX_FEED_NONE;
    case STATE_SYNC_2:
        if (byte == GPACE_UBX_SYNC_2) {
            parser->state = STATE_CLASS;
            parser->ck_a = 0;
            parser->ck_b = 0;
        } else {
            parser->state = (byte == GPACE_UBX_SYNC_1) ? STATE_SYNC_2 : STATE_SYNC_1;
        }
        return GPACE_UBX_FEED_NONE;
    case STATE_CLASS:
        parser->cls = byte;
        fletcher_step(&parser->ck_a, &parser->ck_b, byte);
        parser->state = STATE_ID;
        return GPACE_UBX_FEED_NONE;
    case STATE_ID:
        parser->id = byte;
        fletcher_step(&parser->ck_a, &parser->ck_b, byte);
        parser->state = STATE_LEN_LO;
        return GPACE_UBX_FEED_NONE;
    case STATE_LEN_LO:
        parser->len = byte;
        fletcher_step(&parser->ck_a, &parser->ck_b, byte);
        parser->state = STATE_LEN_HI;
        return GPACE_UBX_FEED_NONE;
    case STATE_LEN_HI:
        parser->len = (uint16_t)(parser->len | ((uint16_t)byte << 8));
        fletcher_step(&parser->ck_a, &parser->ck_b, byte);
        parser->index = 0;
        if (parser->len > GPACE_UBX_MAX_PAYLOAD) {
            parser->oversize++;
            parser->state = STATE_SYNC_1;
            return GPACE_UBX_FEED_OVERSIZE;
        }
        parser->state = (parser->len == 0) ? STATE_CK_A : STATE_PAYLOAD;
        return GPACE_UBX_FEED_NONE;
    case STATE_PAYLOAD:
        parser->payload[parser->index++] = byte;
        fletcher_step(&parser->ck_a, &parser->ck_b, byte);
        if (parser->index >= parser->len) {
            parser->state = STATE_CK_A;
        }
        return GPACE_UBX_FEED_NONE;
    case STATE_CK_A:
        if (byte != parser->ck_a) {
            parser->checksum_errors++;
            parser->state = STATE_SYNC_1;
            return GPACE_UBX_FEED_BAD_CHECKSUM;
        }
        parser->state = STATE_CK_B;
        return GPACE_UBX_FEED_NONE;
    case STATE_CK_B:
        parser->state = STATE_SYNC_1;
        if (byte != parser->ck_b) {
            parser->checksum_errors++;
            return GPACE_UBX_FEED_BAD_CHECKSUM;
        }
        parser->messages++;
        return GPACE_UBX_FEED_MESSAGE;
    default:
        parser->state = STATE_SYNC_1;
        return GPACE_UBX_FEED_NONE;
    }
}

bool gpace_ubx_decode_ack(const gpace_ubx_parser_t *msg, uint8_t *acked_cls, uint8_t *acked_id, bool *is_ack)
{
    if (msg->cls != GPACE_UBX_CLS_ACK || msg->len != 2) {
        return false;
    }
    if (msg->id != GPACE_UBX_ID_ACK_ACK && msg->id != GPACE_UBX_ID_ACK_NAK) {
        return false;
    }
    *acked_cls = msg->payload[0];
    *acked_id = msg->payload[1];
    *is_ack = (msg->id == GPACE_UBX_ID_ACK_ACK);
    return true;
}

bool gpace_ubx_decode_valget(const gpace_ubx_parser_t *msg, uint32_t expected_key, uint64_t *value)
{
    const size_t value_size = gpace_ubx_key_size(expected_key);
    uint64_t result = 0;
    if (msg->cls != GPACE_UBX_CLS_CFG || msg->id != GPACE_UBX_ID_CFG_VALGET || value_size == 0) {
        return false;
    }
    if (msg->len < 4u + 4u + value_size || read_u32(&msg->payload[4]) != expected_key) {
        return false;
    }
    for (size_t i = 0; i < value_size; i++) {
        result |= (uint64_t)msg->payload[8 + i] << (8u * i);
    }
    *value = result;
    return true;
}

bool gpace_ubx_decode_nav_pvt(const uint8_t *payload, uint16_t len, gpace_ubx_pvt_t *out)
{
    if (len < GPACE_UBX_PVT_PAYLOAD_LEN) {
        return false;
    }
    out->itow_ms = read_u32(&payload[0]);
    out->year = read_u16(&payload[4]);
    out->month = payload[6];
    out->day = payload[7];
    out->hour = payload[8];
    out->minute = payload[9];
    out->second = payload[10];
    out->valid = payload[11];
    out->fix_type = payload[20];
    out->flags = payload[21];
    out->num_sv = payload[23];
    out->lon_e7 = read_i32(&payload[24]);
    out->lat_e7 = read_i32(&payload[28]);
    out->height_mm = read_i32(&payload[32]);
    out->hmsl_mm = read_i32(&payload[36]);
    out->hacc_mm = read_u32(&payload[40]);
    out->vacc_mm = read_u32(&payload[44]);
    out->vel_n_mms = read_i32(&payload[48]);
    out->vel_e_mms = read_i32(&payload[52]);
    out->vel_d_mms = read_i32(&payload[56]);
    out->gspeed_mms = read_i32(&payload[60]);
    out->head_mot_e5 = read_i32(&payload[64]);
    out->sacc_mms = read_u32(&payload[68]);
    out->head_acc_e5 = read_u32(&payload[72]);
    out->pdop_e2 = read_u16(&payload[76]);
    return true;
}
