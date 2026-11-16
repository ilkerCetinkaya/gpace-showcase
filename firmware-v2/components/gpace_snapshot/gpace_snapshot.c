#include "gpace_snapshot.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"

static portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t latest[GPACE_FRAME_SIZE];
static uint32_t sequence;

void gpace_snapshot_publish(const uint8_t frame[GPACE_FRAME_SIZE])
{
    portENTER_CRITICAL(&lock);
    memcpy(latest, frame, GPACE_FRAME_SIZE);
    sequence++;
    portEXIT_CRITICAL(&lock);
}

uint32_t gpace_snapshot_copy(uint8_t out[GPACE_FRAME_SIZE])
{
    uint32_t current;
    portENTER_CRITICAL(&lock);
    memcpy(out, latest, GPACE_FRAME_SIZE);
    current = sequence;
    portEXIT_CRITICAL(&lock);
    return current;
}
