/* lvgl_mipi_panel.cpp - see lvgl_mipi_panel.h.
 * SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include <lcdifv2.h>
#include "lvgl_mipi_panel.h"

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

static void mipi_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
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

lv_display_t *lvgl_mipi_panel_create(DisplayClass &display)
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
    lv_display_set_flush_cb(disp, mipi_flush_cb);
    /* Full-screen single buffer: DIRECT mode requires the buffer to be the
     * whole frame, and here it is the scanout buffer itself. PANEL_FB_BYTES is
     * panel_config.h's name for this length -- the same one lcdifv2Begin()
     * allocates and strides the scanout descriptor by -- so it is used rather
     * than re-derived, which is also what keeps the two from drifting apart. */
    lv_display_set_buffers(disp, display.framebuffer(), nullptr, PANEL_FB_BYTES,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    return disp;
}

bool lvgl_mipi_panel_frame_done() { return s_frame_done; }

uint32_t lvgl_mipi_panel_flushed_px() { return s_flushed_px; }

/* --- v5: ISR-signalled flip fence -----------------------------------------
 * OWNERSHIP TABLE -- single writer per field, and it is the design:
 *   field                thread writes            ISR writes
 *   s_db_pending_fb      set (flush_cb), clear    clear (retire)
 *                        (timeout abandon ONLY)
 *   s_db_scanned_fb      --                       set (retire)
 *   s_db_isr_retires     --                       increment
 *   s_db_flips           increment (flush_cb)     --
 *   s_db_vsyncs          increment (flip_sync)    --
 *   s_db_vsync_timeouts  increment (flip_sync)    --
 * Pointer stores are naturally atomic on ARMv7-M (aligned 32-bit).  The one
 * dual-writer field is pending_fb, and its two clears cannot both matter:
 * the timeout abandon runs 40 ms after the set, and an ISR retire racing
 * that exact store either wins (flip landed; the timeout tick miscounts one
 * "timeout" against a landed flip -- self-describing next to a healthy
 * s_db_isr_retires) or loses (true abandon).  Neither corrupts a pointer. */
static const uint16_t *volatile s_db_pending_fb = nullptr;
static const uint16_t *volatile s_db_scanned_fb = nullptr;
static volatile uint32_t s_db_isr_retires = 0;
static uint32_t s_db_flips = 0, s_db_vsyncs = 0, s_db_vsync_timeouts = 0;

/* INTERRUPT CONTEXT.  Flag work only: retire the pending flip.  Runs on
 * EVERY vsync (~60 Hz) once create_db has attached it; the retire fires only
 * when a flip is pending, and s_db_isr_retires counts RETIRES -- one per
 * flip, deterministic -- not ISR entries, which are runtime-dependent. */
static void db_vsync_isr()
{
    const uint16_t *p = s_db_pending_fb;
    if (p) {
        s_db_scanned_fb = p;
        s_db_pending_fb = nullptr;
        s_db_isr_retires++;
    }
}

void lvgl_mipi_panel_flip_sync()
{
    if (s_db_pending_fb == nullptr) return;
    /* Bounded wait on the ISR-retired flag -- v4's 40 ms bound, degraded
     * mode and counters survive the ISR migration unchanged; only the thing
     * polled moved from the device register into RAM the ISR owns. */
    const uint32_t t0 = micros();
    while (s_db_pending_fb != nullptr) {
        if ((uint32_t)(micros() - t0) > 40000u) {
            s_db_vsync_timeouts++;
            s_db_pending_fb = nullptr;   /* thread-side abandon; see table */
            return;
        }
    }
    s_db_vsyncs++;
}

static void db_flush_wait_cb(lv_display_t *disp)
{
    (void)disp;
    lvgl_mipi_panel_flip_sync();
    /* No lv_display_flush_ready() here, and none in db_flush_cb's last-flush
     * branch either: with a flush_wait_cb registered, LVGL ITSELF clears
     * `flushing` the moment this callback returns (lv_refr.c:1435-1440,
     * wait_for_flushing).  Calling flush_ready from the flush_cb instead
     * would clear the flag synchronously, this callback would never run,
     * and the render-into-scanned-buffer hazard would be prevented only by
     * the 33 ms refresh period happening to exceed the flip latency --
     * timing luck, not the invariant.  MEASURED, not theorised: with the
     * synchronous flush_ready the gate's VSYNCS counted 3 (the example's own
     * explicit syncs) instead of one consumed landing per flip. */
}

static void db_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    s_flushed_px += (uint32_t)lv_area_get_size(area);
    if (lv_display_flush_is_last(disp)) {
        /* px_map is the FRAME BASE in DIRECT mode (fact: NXP's port hands
         * color_p straight to setFrameBuffer, lvgl_support.c:426-434) -- the
         * buffer LVGL just finished rendering, which becomes the new front. */
        lcdifv2FlipTo((const uint16_t *)px_map);
        /* Pending-store AFTER FlipTo, same conservative direction as v4's
         * FlipTo-before-Arm: a vsync in the gap means the ISR sees no
         * pending flip and the retire lands one frame later -- an extra
         * frame of wait, never a false pass. */
        s_db_pending_fb = (const uint16_t *)px_map;
        s_db_flips++;
        s_frame_done = true;
        /* Deliberately NO flush_ready on the last flush: `flushing` stays
         * set until LVGL next needs the buffer, at which point it invokes
         * db_flush_wait_cb above and clears the flag itself.  That deferral
         * IS the invariant's enforcement -- see the wait_cb comment. */
        return;
    }
    /* Non-last flushes have nothing to wait for. */
    lv_display_flush_ready(disp);
}

lv_display_t *lvgl_mipi_panel_create_db(DisplayClass &display)
{
    LV_ASSERT_NULL(display.framebuffer());   /* Display.begin() must have succeeded */
    LV_ASSERT(display.width() == PANEL_WIDTH && display.height() == PANEL_HEIGHT);

    uint16_t *alt = lcdifv2AllocAltFramebuffer();
    LV_ASSERT_NULL(alt);                     /* no silent single-buffer fallback */

    s_frame_done = false;
    s_flushed_px = 0;
    s_db_pending_fb = nullptr;
    s_db_scanned_fb = nullptr;
    s_db_isr_retires = 0;
    s_db_flips = s_db_vsyncs = s_db_vsync_timeouts = 0;

    lv_display_t *disp = lv_display_create((int32_t)display.width(),
                                           (int32_t)display.height());
    lv_display_set_flush_cb(disp, db_flush_cb);
    lv_display_set_flush_wait_cb(disp, db_flush_wait_cb);
    /* buf1 = ALT (LVGL renders it first), buf2 = the live scanout buffer.
     * See the header: this order is load-bearing. */
    lv_display_set_buffers(disp, alt, display.framebuffer(), PANEL_FB_BYTES,
                           LV_DISPLAY_RENDER_MODE_DIRECT);
    /* Attach LAST, after all state is initialised -- the ISR runs from the
     * next vsync on. */
    lcdifv2AttachVsyncInterrupt(db_vsync_isr);
    return disp;
}

uint32_t lvgl_mipi_panel_flips()          { return s_db_flips; }
uint32_t lvgl_mipi_panel_vsyncs()         { return s_db_vsyncs; }
uint32_t lvgl_mipi_panel_vsync_timeouts() { return s_db_vsync_timeouts; }
uint32_t lvgl_mipi_panel_vsync_isrs()     { return s_db_isr_retires; }
const uint16_t *lvgl_mipi_panel_scanned_fb() { return (const uint16_t *)s_db_scanned_fb; }
