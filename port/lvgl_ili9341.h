/* lvgl_ili9341.h - LVGL display binding for the ILI9341 (SPI, 320x240).
 * SPDX-License-Identifier: MIT */
#pragma once
#include <ILI9341_t3.h>
#include "lvgl_rt1176.h"

#define LVGL_ILI9341_W 320
#define LVGL_ILI9341_H 240

/* Creates the lv_display_t. buf1/buf2 are caller-owned RGB565 partial-render
 * buffers of `px` pixels each (buf2 may be nullptr). Partial mode: LVGL renders
 * a slice at a time and flush_cb blits it with writeRect.
 *
 * BUFFER ALIGNMENT: buf1/buf2 must be at least LV_DRAW_BUF_ALIGN-aligned.
 * lv_display_set_buffers() uses the pointers exactly as given and does not
 * re-align them, and a uint16_t* only guarantees 2-byte alignment on its own --
 * so this is the CALLER's contract, not something this function fixes up.
 * 32-byte (cache-line) alignment is recommended: it satisfies the requirement
 * today and lets a future DMA blit path reuse the same buffers unchanged.
 *
 * PANEL ORIENTATION: the caller must already have put the panel into a 320x240
 * (landscape) orientation -- e.g. tft.setRotation(1) -- before calling this.
 * The dimensions above are hardcoded and nothing here re-checks the panel. */
lv_display_t *lvgl_ili9341_create(ILI9341_t3 &tft, uint16_t *buf1, uint16_t *buf2, size_t px);

/* True once a full screen refresh has been flushed. LATCHING and ONE-SHOT: set
 * by the last flush of the first full refresh and cleared only by another
 * create(). This is enough to sequence a gate ("has one frame rendered yet?")
 * and is NOT a per-frame completion signal -- a caller that needs one wants a
 * counter or a callback, not this. */
bool lvgl_ili9341_frame_done();
