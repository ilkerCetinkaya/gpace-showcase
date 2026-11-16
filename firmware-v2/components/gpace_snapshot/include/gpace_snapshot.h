#ifndef GPACE_SNAPSHOT_H
#define GPACE_SNAPSHOT_H

#include <stdint.h>

#include "gpace_frame.h"

void gpace_snapshot_publish(const uint8_t frame[GPACE_FRAME_SIZE]);
uint32_t gpace_snapshot_copy(uint8_t out[GPACE_FRAME_SIZE]);

#endif
