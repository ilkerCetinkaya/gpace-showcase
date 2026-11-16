#ifndef FAKE_GNSS_H
#define FAKE_GNSS_H

#include <stdbool.h>
#include <stdint.h>

#include "gpace_ubx_session.h"

#define FAKE_MAX_KEYS 32
#define FAKE_RX_CAPACITY 4096

typedef struct {
    uint32_t keys[FAKE_MAX_KEYS];
    uint64_t values[FAKE_MAX_KEYS];
    int key_count;
    uint32_t nak_keys[4];
    int nak_count;
    uint32_t device_baud;
    uint32_t pending_baud;
    uint32_t host_baud;
    uint32_t time_ms;
    uint32_t read_chunk;
    uint8_t rx[FAKE_RX_CAPACITY];
    int rx_len;
    int rx_pos;
    gpace_ubx_parser_t parser;
    uint32_t pvt_sent;
    gpace_ubx_link_t link;
} fake_gnss_t;

void fake_gnss_init(fake_gnss_t *fake, uint32_t device_baud);
uint64_t fake_gnss_value(const fake_gnss_t *fake, uint32_t key, bool *found);

#endif
