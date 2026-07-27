/* lvgl_ili9341.cpp - see lvgl_ili9341.h.
 * SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "lvgl_ili9341.h"

/* The flush below hashes and blits w*h*2 CONTIGUOUS bytes, which is only the
 * frame's pixels if the draw buffer has no stride padding and 2 bytes/px. */
static_assert(LV_DRAW_BUF_STRIDE_ALIGN == 1, "partial flush assumes unpadded stride");
static_assert(LV_COLOR_DEPTH == 16, "flush assumes RGB565");

static ILI9341_t3 *s_tft = nullptr;
/* Not volatile: with LV_USE_OS == LV_OS_NONE, flush_cb runs only from
 * lv_timer_handler() and the reader is the same thread context, so there is no
 * concurrent writer to guard against. A future DMA-completion-ISR path that
 * sets this from an interrupt would need volatile (or an atomic) added back. */
static bool s_frame_done = false;

static void ili9341_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    /* Feed the gate oracle BEFORE the blit: this is the renderer's output,
     * independent of whether any panel is present (QEMU models none). */
    lvgl_sum_feed(px_map, (size_t)w * (size_t)h * 2u);

    LV_ASSERT_NULL(s_tft);   /* set by create(), the only site registering this cb */
    s_tft->writeRect(area->x1, area->y1, w, h, (const uint16_t *)px_map);

    if (lv_display_flush_is_last(disp)) s_frame_done = true;
    lv_display_flush_ready(disp);
}

lv_display_t *lvgl_ili9341_create(ILI9341_t3 &tft, uint16_t *buf1, uint16_t *buf2, size_t px)
{
    s_tft = &tft;
    s_frame_done = false;
    lv_display_t *disp = lv_display_create(LVGL_ILI9341_W, LVGL_ILI9341_H);
    lv_display_set_flush_cb(disp, ili9341_flush_cb);
    lv_display_set_buffers(disp, buf1, buf2, (uint32_t)(px * 2u),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    return disp;
}

bool lvgl_ili9341_frame_done() { return s_frame_done; }
