#ifndef BLE_PAYLOAD_H
#define BLE_PAYLOAD_H

#include "ubx_gpace.h"
#include "common.h"
#include "gatt_svc_gpace.h"
#include "imu.h"
#include "init.h"

union val16_to_val8
{
    uint8_t val8[2];
    int16_t val16;
};

int get_data(uint8_t* buffer);
int checksum(uint8_t* checksum_a, uint8_t* checksum_b, uint8_t* buffer);

#endif