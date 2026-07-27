/* lvgl_rt1176.cpp - see lvgl_rt1176.h.
 * SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "lvgl_rt1176.h"
#include "lvgl_rt1176_assert.h"

static const uint32_t FNV1A_OFFSET = 2166136261u;
static const uint32_t FNV1A_PRIME  = 16777619u;
static uint32_t s_sum   = FNV1A_OFFSET;
static uint32_t s_bytes = 0;

static uint32_t lvgl_tick_cb() { return (uint32_t)millis(); }

void lvgl_rt1176_begin()
{
    lv_init();
    lv_tick_set_cb(lvgl_tick_cb);
}

void lvgl_rt1176_loop() { lv_timer_handler(); }

void lvgl_sum_reset() { s_sum = FNV1A_OFFSET; s_bytes = 0; }

void lvgl_sum_feed(const void *data, size_t bytes)
{
    /* uint8_t, not char: on a signed-char ABI a char would sign-extend before
     * the xor and corrupt the hash for every byte >= 0x80. */
    const uint8_t *p = (const uint8_t *)data;
    for (size_t i = 0; i < bytes; i++) { s_sum ^= p[i]; s_sum *= FNV1A_PRIME; }
    s_bytes += (uint32_t)bytes;
}

uint32_t lvgl_sum_value() { return s_sum; }

uint32_t lvgl_sum_bytes() { return s_bytes; }

void lvgl_rt1176_assert_report(void)
{
    Serial1.println("LVGL_ASSERT!");
    Serial1.flush();
}
