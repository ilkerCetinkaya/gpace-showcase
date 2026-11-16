#include "fake_gnss.h"

#include <string.h>

static uint64_t *slot_for(fake_gnss_t *fake, uint32_t key, bool create)
{
    for (int i = 0; i < fake->key_count; i++) {
        if (fake->keys[i] == key) {
            return &fake->values[i];
        }
    }
    if (!create || fake->key_count >= FAKE_MAX_KEYS) {
        return NULL;
    }
    fake->keys[fake->key_count] = key;
    fake->values[fake->key_count] = 0;
    return &fake->values[fake->key_count++];
}

uint64_t fake_gnss_value(const fake_gnss_t *fake, uint32_t key, bool *found)
{
    for (int i = 0; i < fake->key_count; i++) {
        if (fake->keys[i] == key) {
            *found = true;
            return fake->values[i];
        }
    }
    *found = false;
    return 0;
}

static void queue_bytes(fake_gnss_t *fake, const uint8_t *data, size_t len)
{
    if (fake->rx_pos > 0 && fake->rx_pos == fake->rx_len) {
        fake->rx_len = 0;
        fake->rx_pos = 0;
    }
    if (fake->rx_len + (int)len <= FAKE_RX_CAPACITY) {
        memcpy(&fake->rx[fake->rx_len], data, len);
        fake->rx_len += (int)len;
    }
}

static void queue_message(fake_gnss_t *fake, uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[GPACE_UBX_FRAME_OVERHEAD + GPACE_UBX_MAX_PAYLOAD];
    const size_t total = gpace_ubx_encode(cls, id, payload, len, frame, sizeof(frame));
    queue_bytes(fake, frame, total);
}

static void queue_pvt(fake_gnss_t *fake)
{
    uint8_t payload[GPACE_UBX_PVT_PAYLOAD_LEN] = {0};
    const uint32_t itow = 1000u + fake->pvt_sent * 40u;
    payload[0] = (uint8_t)itow;
    payload[1] = (uint8_t)(itow >> 8);
    payload[9] = 30;
    payload[10] = (uint8_t)(fake->pvt_sent % 60);
    payload[20] = 3;
    payload[23] = 14;
    payload[24] = 0x80; payload[25] = 0x96; payload[26] = 0x98; payload[27] = 0x00;
    payload[28] = 0x00; payload[29] = 0x6C; payload[30] = 0xDC; payload[31] = 0x02;
    payload[60] = 0xE8; payload[61] = 0x03;
    fake->pvt_sent++;
    queue_message(fake, GPACE_UBX_CLS_NAV, GPACE_UBX_ID_NAV_PVT, payload, sizeof(payload));
}

static bool is_nak_key(const fake_gnss_t *fake, uint32_t key)
{
    for (int i = 0; i < fake->nak_count; i++) {
        if (fake->nak_keys[i] == key) {
            return true;
        }
    }
    return false;
}

static uint32_t get_key(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void answer(fake_gnss_t *fake)
{
    const gpace_ubx_parser_t *msg = &fake->parser;
    uint8_t ack[2] = { msg->cls, msg->id };

    if (msg->cls == GPACE_UBX_CLS_CFG && msg->id == GPACE_UBX_ID_CFG_VALSET && msg->len >= 8) {
        const uint32_t key = get_key(&msg->payload[4]);
        const size_t size = gpace_ubx_key_size(key);
        uint64_t value = 0;
        if (size == 0 || msg->len != 8 + size || is_nak_key(fake, key)) {
            queue_message(fake, GPACE_UBX_CLS_ACK, GPACE_UBX_ID_ACK_NAK, ack, 2);
            return;
        }
        for (size_t i = 0; i < size; i++) {
            value |= (uint64_t)msg->payload[8 + i] << (8u * i);
        }
        *slot_for(fake, key, true) = value;
        queue_message(fake, GPACE_UBX_CLS_ACK, GPACE_UBX_ID_ACK_ACK, ack, 2);
        if (key == GPACE_UBX_KEY_UART1_BAUDRATE) {
            fake->pending_baud = (uint32_t)value;
        }
    } else if (msg->cls == GPACE_UBX_CLS_CFG && msg->id == GPACE_UBX_ID_CFG_VALGET && msg->len >= 8) {
        const uint32_t key = get_key(&msg->payload[4]);
        const size_t size = gpace_ubx_key_size(key);
        uint64_t *slot = slot_for(fake, key, false);
        uint8_t payload[16] = {0};
        if (slot == NULL || size == 0) {
            queue_message(fake, GPACE_UBX_CLS_ACK, GPACE_UBX_ID_ACK_NAK, ack, 2);
            return;
        }
        payload[4] = (uint8_t)key;
        payload[5] = (uint8_t)(key >> 8);
        payload[6] = (uint8_t)(key >> 16);
        payload[7] = (uint8_t)(key >> 24);
        for (size_t i = 0; i < size; i++) {
            payload[8 + i] = (uint8_t)(*slot >> (8u * i));
        }
        queue_message(fake, GPACE_UBX_CLS_CFG, GPACE_UBX_ID_CFG_VALGET, payload, (uint16_t)(8 + size));
    } else if (msg->cls == GPACE_UBX_CLS_NAV && msg->id == GPACE_UBX_ID_NAV_PVT && msg->len == 0) {
        queue_pvt(fake);
    }
}

static int fake_write(void *ctx, const uint8_t *data, size_t len)
{
    fake_gnss_t *fake = ctx;
    if (fake->host_baud != fake->device_baud) {
        return (int)len;
    }
    for (size_t i = 0; i < len; i++) {
        if (gpace_ubx_parser_feed(&fake->parser, data[i]) == GPACE_UBX_FEED_MESSAGE) {
            answer(fake);
        }
    }
    return (int)len;
}

static int fake_read(void *ctx, uint8_t *data, size_t capacity, uint32_t timeout_ms)
{
    fake_gnss_t *fake = ctx;
    int available;
    int count;
    bool found = false;
    uint64_t auto_pvt;

    if (fake->host_baud != fake->device_baud) {
        fake->time_ms += timeout_ms;
        return 0;
    }
    auto_pvt = fake_gnss_value(fake, GPACE_UBX_KEY_MSGOUT_NAV_PVT_UART1, &found);
    if (fake->rx_pos >= fake->rx_len && found && auto_pvt != 0) {
        queue_pvt(fake);
        fake->time_ms += 40;
    }
    available = fake->rx_len - fake->rx_pos;
    if (available <= 0) {
        fake->time_ms += timeout_ms;
        return 0;
    }
    count = available < (int)capacity ? available : (int)capacity;
    if (fake->read_chunk > 0 && count > (int)fake->read_chunk) {
        count = (int)fake->read_chunk;
    }
    memcpy(data, &fake->rx[fake->rx_pos], (size_t)count);
    fake->rx_pos += count;
    fake->time_ms += 1;
    if (fake->rx_pos >= fake->rx_len && fake->pending_baud != 0) {
        fake->device_baud = fake->pending_baud;
        fake->pending_baud = 0;
    }
    return count;
}

static int fake_set_baud(void *ctx, uint32_t baud)
{
    fake_gnss_t *fake = ctx;
    fake->host_baud = baud;
    return 0;
}

static void fake_flush(void *ctx)
{
    fake_gnss_t *fake = ctx;
    fake->rx_len = 0;
    fake->rx_pos = 0;
}

static uint32_t fake_now(void *ctx)
{
    return ((fake_gnss_t *)ctx)->time_ms;
}

static void fake_delay(void *ctx, uint32_t ms)
{
    ((fake_gnss_t *)ctx)->time_ms += ms;
}

void fake_gnss_init(fake_gnss_t *fake, uint32_t device_baud)
{
    memset(fake, 0, sizeof(*fake));
    fake->device_baud = device_baud;
    gpace_ubx_parser_reset(&fake->parser);
    *slot_for(fake, GPACE_UBX_KEY_RATE_MEAS, true) = 100;
    *slot_for(fake, GPACE_UBX_KEY_MSGOUT_NAV_PVT_UART1, true) = 0;
    fake->link.ctx = fake;
    fake->link.write = fake_write;
    fake->link.read = fake_read;
    fake->link.set_baud = fake_set_baud;
    fake->link.flush = fake_flush;
    fake->link.now_ms = fake_now;
    fake->link.delay_ms = fake_delay;
}
