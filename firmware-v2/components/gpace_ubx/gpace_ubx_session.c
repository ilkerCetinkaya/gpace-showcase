#include "gpace_ubx_session.h"

#include <string.h>

#define READ_CHUNK 64u
#define READ_SLICE_MS 20u

void gpace_ubx_config_default(gpace_ubx_config_t *config)
{
    config->meas_period_ms = 40;
    config->dynamic_model = GPACE_UBX_DYNMODEL_AUTOMOTIVE;
    config->gps = true;
    config->galileo = true;
    config->beidou = true;
    config->glonass = true;
    config->qzss = true;
    config->sbas = true;
    config->auto_pvt = true;
    config->baud = 460800;
}

void gpace_ubx_init(gpace_ubx_t *ubx, const gpace_ubx_link_t *link)
{
    memset(ubx, 0, sizeof(*ubx));
    ubx->link = link;
    gpace_ubx_parser_reset(&ubx->parser);
}

typedef enum {
    WAIT_ACK,
    WAIT_VALGET,
    WAIT_PVT
} wait_kind_t;

typedef struct {
    wait_kind_t kind;
    uint8_t ack_cls;
    uint8_t ack_id;
    uint32_t key;
    uint64_t *value;
    gpace_ubx_pvt_t *pvt;
    bool ack_result;
} wait_t;

static bool handle_message(gpace_ubx_t *ubx, wait_t *wait)
{
    const gpace_ubx_parser_t *msg = &ubx->parser;
    uint8_t acked_cls = 0;
    uint8_t acked_id = 0;
    bool is_ack = false;

    if (msg->cls == GPACE_UBX_CLS_NAV && msg->id == GPACE_UBX_ID_NAV_PVT) {
        gpace_ubx_pvt_t decoded;
        if (gpace_ubx_decode_nav_pvt(msg->payload, msg->len, &decoded)) {
            ubx->pvt_count++;
            if (wait->kind == WAIT_PVT && wait->pvt != NULL) {
                *wait->pvt = decoded;
                return true;
            }
        }
        return false;
    }
    if (wait->kind == WAIT_ACK && gpace_ubx_decode_ack(msg, &acked_cls, &acked_id, &is_ack)) {
        if (acked_cls == wait->ack_cls && acked_id == wait->ack_id) {
            wait->ack_result = is_ack;
            return true;
        }
        return false;
    }
    if (wait->kind == WAIT_VALGET && gpace_ubx_decode_valget(msg, wait->key, wait->value)) {
        wait->ack_result = true;
        return true;
    }
    if (wait->kind == WAIT_VALGET && msg->cls == GPACE_UBX_CLS_ACK && msg->id == GPACE_UBX_ID_ACK_NAK) {
        wait->ack_result = false;
        return true;
    }
    return false;
}

static bool pump(gpace_ubx_t *ubx, wait_t *wait, uint32_t timeout_ms)
{
    uint8_t chunk[READ_CHUNK];
    const uint32_t start = ubx->link->now_ms(ubx->link->ctx);

    for (;;) {
        const uint32_t elapsed = ubx->link->now_ms(ubx->link->ctx) - start;
        uint32_t slice;
        int count;
        if (elapsed >= timeout_ms) {
            return false;
        }
        slice = timeout_ms - elapsed;
        if (slice > READ_SLICE_MS) {
            slice = READ_SLICE_MS;
        }
        count = ubx->link->read(ubx->link->ctx, chunk, sizeof(chunk), slice);
        for (int i = 0; i < count; i++) {
            if (gpace_ubx_parser_feed(&ubx->parser, chunk[i]) == GPACE_UBX_FEED_MESSAGE && handle_message(ubx, wait)) {
                return true;
            }
        }
    }
}

static bool send_frame(gpace_ubx_t *ubx, const uint8_t *frame, size_t len)
{
    return len > 0 && ubx->link->write(ubx->link->ctx, frame, len) == (int)len;
}

bool gpace_ubx_valset(gpace_ubx_t *ubx, uint8_t layers, uint32_t key, uint64_t value, uint32_t timeout_ms)
{
    uint8_t frame[GPACE_UBX_FRAME_OVERHEAD + 16];
    wait_t wait = { .kind = WAIT_ACK, .ack_cls = GPACE_UBX_CLS_CFG, .ack_id = GPACE_UBX_ID_CFG_VALSET };
    const size_t len = gpace_ubx_build_valset(layers, key, value, frame, sizeof(frame));

    if (!send_frame(ubx, frame, len)) {
        return false;
    }
    return pump(ubx, &wait, timeout_ms) && wait.ack_result;
}

bool gpace_ubx_valget(gpace_ubx_t *ubx, uint32_t key, uint64_t *value, uint32_t timeout_ms)
{
    uint8_t frame[GPACE_UBX_FRAME_OVERHEAD + 8];
    wait_t wait = { .kind = WAIT_VALGET, .key = key, .value = value };
    const size_t len = gpace_ubx_build_valget(GPACE_UBX_VALGET_LAYER_RAM, key, frame, sizeof(frame));

    if (!send_frame(ubx, frame, len)) {
        return false;
    }
    return pump(ubx, &wait, timeout_ms) && wait.ack_result;
}

bool gpace_ubx_connect(gpace_ubx_t *ubx, const uint32_t *bauds, size_t baud_count, uint32_t *found_baud)
{
    uint64_t scratch = 0;

    for (size_t i = 0; i < baud_count; i++) {
        if (ubx->link->set_baud(ubx->link->ctx, bauds[i]) != 0) {
            continue;
        }
        ubx->link->flush(ubx->link->ctx);
        gpace_ubx_parser_reset(&ubx->parser);
        if (gpace_ubx_valget(ubx, GPACE_UBX_KEY_RATE_MEAS, &scratch, 250)) {
            if (found_baud != NULL) {
                *found_baud = bauds[i];
            }
            return true;
        }
    }
    return false;
}

bool gpace_ubx_needs_config(gpace_ubx_t *ubx, const gpace_ubx_config_t *config)
{
    uint64_t value = 0;
    if (!gpace_ubx_valget(ubx, GPACE_UBX_KEY_RATE_MEAS, &value, 300)) {
        return true;
    }
    if (value != config->meas_period_ms) {
        return true;
    }
    if (!gpace_ubx_valget(ubx, GPACE_UBX_KEY_MSGOUT_NAV_PVT_UART1, &value, 300)) {
        return true;
    }
    return (value != 0) != config->auto_pvt;
}

static void record(gpace_ubx_report_t *report, uint32_t key, bool ok)
{
    report->attempted++;
    if (ok) {
        report->acknowledged++;
    } else if (report->failed_count < sizeof(report->failed_keys) / sizeof(report->failed_keys[0])) {
        report->failed_keys[report->failed_count++] = key;
    }
}

bool gpace_ubx_apply_config(gpace_ubx_t *ubx, const gpace_ubx_config_t *config, gpace_ubx_report_t *report)
{
    const uint8_t layers = GPACE_UBX_LAYER_RAM | GPACE_UBX_LAYER_BBR;
    const struct {
        uint32_t key;
        uint64_t value;
    } items[] = {
        { GPACE_UBX_KEY_UART1OUTPROT_NMEA, 0 },
        { GPACE_UBX_KEY_UART1INPROT_UBX, 1 },
        { GPACE_UBX_KEY_UART1OUTPROT_UBX, 1 },
        { GPACE_UBX_KEY_SIGNAL_GPS_ENA, config->gps },
        { GPACE_UBX_KEY_SIGNAL_GAL_ENA, config->galileo },
        { GPACE_UBX_KEY_SIGNAL_BDS_ENA, config->beidou },
        { GPACE_UBX_KEY_SIGNAL_GLO_ENA, config->glonass },
        { GPACE_UBX_KEY_SIGNAL_QZSS_ENA, config->qzss },
        { GPACE_UBX_KEY_SIGNAL_SBAS_ENA, config->sbas },
        { GPACE_UBX_KEY_NAVSPG_DYNMODEL, config->dynamic_model },
        { GPACE_UBX_KEY_RATE_MEAS, config->meas_period_ms },
        { GPACE_UBX_KEY_RATE_NAV, 1 },
        { GPACE_UBX_KEY_MSGOUT_NAV_PVT_UART1, config->auto_pvt ? 1 : 0 },
    };
    bool all_ok = true;

    memset(report, 0, sizeof(*report));
    for (size_t i = 0; i < sizeof(items) / sizeof(items[0]); i++) {
        const bool ok = gpace_ubx_valset(ubx, layers, items[i].key, items[i].value, 500);
        record(report, items[i].key, ok);
        all_ok = all_ok && ok;
        ubx->link->delay_ms(ubx->link->ctx, 5);
    }
    if (config->baud != 0) {
        const bool ok = gpace_ubx_valset(ubx, layers, GPACE_UBX_KEY_UART1_BAUDRATE, config->baud, 500);
        record(report, GPACE_UBX_KEY_UART1_BAUDRATE, ok);
        all_ok = all_ok && ok;
        if (ok) {
            ubx->link->delay_ms(ubx->link->ctx, 20);
            ubx->link->set_baud(ubx->link->ctx, config->baud);
            ubx->link->flush(ubx->link->ctx);
            gpace_ubx_parser_reset(&ubx->parser);
            report->baud_changed = true;
        }
    }
    return all_ok;
}

bool gpace_ubx_request_pvt(gpace_ubx_t *ubx)
{
    uint8_t frame[GPACE_UBX_FRAME_OVERHEAD];
    const size_t len = gpace_ubx_build_poll(GPACE_UBX_CLS_NAV, GPACE_UBX_ID_NAV_PVT, frame, sizeof(frame));
    return send_frame(ubx, frame, len);
}

bool gpace_ubx_poll(gpace_ubx_t *ubx, gpace_ubx_pvt_t *pvt, uint32_t timeout_ms)
{
    wait_t wait = { .kind = WAIT_PVT, .pvt = pvt };
    const bool got = pump(ubx, &wait, timeout_ms);
    if (got) {
        ubx->last_pvt_ms = ubx->link->now_ms(ubx->link->ctx);
    }
    return got;
}
