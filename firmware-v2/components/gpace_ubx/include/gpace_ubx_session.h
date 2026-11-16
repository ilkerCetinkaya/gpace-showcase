#ifndef GPACE_UBX_SESSION_H
#define GPACE_UBX_SESSION_H

#include "gpace_ubx.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *ctx;
    int (*write)(void *ctx, const uint8_t *data, size_t len);
    int (*read)(void *ctx, uint8_t *data, size_t capacity, uint32_t timeout_ms);
    int (*set_baud)(void *ctx, uint32_t baud);
    void (*flush)(void *ctx);
    uint32_t (*now_ms)(void *ctx);
    void (*delay_ms)(void *ctx, uint32_t ms);
} gpace_ubx_link_t;

typedef struct {
    const gpace_ubx_link_t *link;
    gpace_ubx_parser_t parser;
    uint32_t last_pvt_ms;
    uint32_t pvt_count;
} gpace_ubx_t;

typedef struct {
    uint16_t meas_period_ms;
    uint8_t dynamic_model;
    bool gps;
    bool galileo;
    bool beidou;
    bool glonass;
    bool qzss;
    bool sbas;
    bool auto_pvt;
    uint32_t baud;
} gpace_ubx_config_t;

typedef struct {
    uint8_t attempted;
    uint8_t acknowledged;
    uint32_t failed_keys[16];
    uint8_t failed_count;
    bool baud_changed;
} gpace_ubx_report_t;

void gpace_ubx_config_default(gpace_ubx_config_t *config);
void gpace_ubx_init(gpace_ubx_t *ubx, const gpace_ubx_link_t *link);

bool gpace_ubx_valset(gpace_ubx_t *ubx, uint8_t layers, uint32_t key, uint64_t value, uint32_t timeout_ms);
bool gpace_ubx_valget(gpace_ubx_t *ubx, uint32_t key, uint64_t *value, uint32_t timeout_ms);
bool gpace_ubx_connect(gpace_ubx_t *ubx, const uint32_t *bauds, size_t baud_count, uint32_t *found_baud);
bool gpace_ubx_apply_config(gpace_ubx_t *ubx, const gpace_ubx_config_t *config, gpace_ubx_report_t *report);
bool gpace_ubx_needs_config(gpace_ubx_t *ubx, const gpace_ubx_config_t *config);

bool gpace_ubx_poll(gpace_ubx_t *ubx, gpace_ubx_pvt_t *pvt, uint32_t timeout_ms);
bool gpace_ubx_request_pvt(gpace_ubx_t *ubx);

#ifdef __cplusplus
}
#endif

#endif
