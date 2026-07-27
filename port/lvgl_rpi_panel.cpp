/* lvgl_rpi_panel.cpp - see lvgl_rpi_panel.h.
 * SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "lvgl_rpi_panel.h"

/* DIRECT mode makes LVGL address the scanout framebuffer with ITS OWN stride,
 * so these two assumptions are load-bearing in a way they are not for a
 * blitting binding: a padded draw-buffer stride here would not merely produce a
 * wrong checksum, it would write skewed pixels into the buffer the LCDIFv2 is
 * displaying. LVGL's stride must match the panel pitch exactly, which for an
 * unpadded PANEL_PITCH_BYTES means no draw-buffer stride padding and 2 B/px. */
static_assert(LV_DRAW_BUF_STRIDE_ALIGN == 1, "direct render assumes unpadded stride");
static_assert(LV_COLOR_DEPTH == 16, "direct render assumes RGB565");
static_assert(PANEL_PITCH_BYTES == PANEL_WIDTH * 2u,
              "direct render draws at LVGL's own stride; a padded panel pitch "
              "would skew every line and must be handled explicitly");

/* Not volatile: with LV_USE_OS == LV_OS_NONE, flush_cb runs only from
 * lv_timer_handler() and the reader is the same thread context, so there is no
 * concurrent writer to guard against. A future vsync-ISR path that sets this
 * from an interrupt would need volatile (or an atomic) added back. */
static bool s_frame_done = false;
/* Same threading rationale as s_frame_done, hence also not volatile. */
static uint32_t s_flushed_px = 0;

static void rpi_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    /* Nothing to transfer: px_map IS the live scanout buffer, and `area` is the
     * region LVGL already drew there. No copy, and deliberately no cache
     * maintenance -- SDRAM/EXTMEM is mapped non-cacheable on this core and the
     * arm_dcache_* helpers are no-ops, so a clean/invalidate here would be
     * cargo cult (hardware-established during the camera bring-up).
     *
     * FORWARD HAZARD: if the D-cache is ever enabled over SDRAM on this core,
     * THIS is the site that must gain an arm_dcache_flush_delete_by_addr() (a
     * clean by address) over `area` before the LCDIFv2 scans it out -- the
     * renderer writes through the cache but the display controller reads
     * memory. That failure would be INVISIBLE in QEMU, which models no cache:
     * every gate here would stay green and it would show up only as corruption
     * on the glass. Treat enabling the cache and revisiting this as one change.
     *
     * Nor is the checksum fed here, unlike the ILI9341 binding: in DIRECT mode
     * a flush may carry only the dirty area, so per-flush hashing would not
     * cover a whole frame. The example checksums the finished framebuffer
     * instead. What IS accumulated is the flushed AREA -- see the header: it is
     * the only available evidence that LVGL redrew the whole screen rather
     * than a corner of it. */
    (void)px_map;
    s_flushed_px += (uint32_t)lv_area_get_size(area);

    if (lv_display_flush_is_last(disp)) s_frame_done = true;
    lv_display_flush_ready(disp);
}

lv_display_t *lvgl_rpi_panel_create(DisplayClass &display)
{
    /* Enforce the header's precondition rather than only documenting it: a
     * failed Display.begin() leaves framebuffer() null, and handing LVGL a null
     * draw buffer faults somewhere inside the renderer with no token to explain
     * it. LV_ASSERT_HANDLER prints one first (lvgl_rt1176_assert.h). */
    LV_ASSERT_NULL(display.framebuffer());   /* Display.begin() must have succeeded; see header */
    /* The static_asserts above are written against the PANEL_* constants, but
     * the display is sized from the runtime accessors. Identical today; this is
     * what catches them diverging if DisplayClass ever gains runtime geometry. */
    LV_ASSERT(display.width() == PANEL_WIDTH && display.height() == PANEL_HEIGHT);

    s_frame_done = false;
    s_flushed_px = 0;
    lv_display_t *disp = lv_display_create((int32_t)display.width(),
                                           (int32_t)display.height());
    lv_display_set_flush_cb(disp, rpi_flush_cb);
    /* Full-screen single buffer: DIRECT mode requires the buffer to be the
     * whole frame, and here it is the scanout buffer itself. PANEL_FB_BYTES is
     * display_timing.h's name for this length -- the same one lcdifv2Begin()
     * allocates and strides the scanout descriptor by -- so it is used rather
     * than re-derived, which is also what keeps the two from drifting apart. */
    lv_display_set_buffers(disp, display.framebuffer(), nullptr, PANEL_FB_BYTES,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    return disp;
}

bool lvgl_rpi_panel_frame_done() { return s_frame_done; }

uint32_t lvgl_rpi_panel_flushed_px() { return s_flushed_px; }
