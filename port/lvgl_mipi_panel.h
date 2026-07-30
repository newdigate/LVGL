/* lvgl_mipi_panel.h - LVGL display binding for a MIPI-DSI panel on the i.MX
 * RT1176 (RGB565, MipiDisplay/LCDIFv2). Panel-neutral: the geometry comes from
 * the DisplayClass instance and from the selected panel's panel_config.h, so
 * this binding serves whichever panel MipiDisplay was built for.
 * SPDX-License-Identifier: MIT */
#pragma once
#include <Display.h>
#include "lvgl_rt1176.h"

/* Creates the lv_display_t over the panel's EXISTING framebuffer.
 *
 * DIRECT RENDER MODE -- THERE IS NO BLIT. Unlike the ILI9341 binding (partial
 * mode + writeRect over SPI), this one hands LVGL the very buffer LCDIFv2 is
 * scanning out, so the renderer's output is already on its way to the glass the
 * moment it is written. flush_cb has nothing to transfer: it only latches
 * "a full refresh finished" and calls lv_display_flush_ready().
 *
 * TEARING: v1 accepts it. LVGL draws into the live scanout buffer with no
 * vsync fence and no back buffer, so a refresh that straddles a scanout can be
 * shown half-old/half-new. Double buffering + a vsync flip is a later
 * milestone; do not bolt one on here without one.
 *
 * BUFFER OWNERSHIP: the framebuffer belongs to MipiDisplay -- it is allocated
 * and 64-byte-aligned by lcdifv2Begin() and is the scanout descriptor's base.
 * The caller neither supplies nor frees it, and must NOT call this before a
 * successful Display.begin(): framebuffer() is nullptr until then.
 *
 * GEOMETRY comes from display.width()/height(), not from a constant here, so
 * the lv_display_t cannot silently disagree with the scanout descriptor.
 *
 * PRECONDITION (asserted, not merely documented): Display.begin() must have
 * succeeded. framebuffer() is nullptr until it does, and the geometry must
 * match the panel the static_asserts in the .cpp are written against. */
lv_display_t *lvgl_mipi_panel_create(DisplayClass &display);

/* True once a full screen refresh has been flushed. LATCHING and ONE-SHOT: set
 * by the last flush of the first full refresh and cleared only by another
 * create(). This is enough to sequence a gate ("has one frame rendered yet?")
 * and is NOT a per-frame completion signal -- a caller that needs one wants a
 * counter or a callback, not this. */
bool lvgl_mipi_panel_frame_done();

/* Total pixel area handed to flush_cb since the last create(), summed over
 * every flush. This is the gate's evidence that LVGL actually invalidated and
 * redrew the WHOLE screen: in DIRECT mode there is no blit whose size could be
 * checked, and the frame checksum cannot tell "LVGL only repainted a corner"
 * from "the scene legitimately changed" -- both just yield a different number.
 * A full refresh of an N-pixel panel totals exactly N: LVGL flushes each
 * invalidated area (subdivided into tiles) once, and those areas are disjoint.
 *
 * It keeps counting after that first frame, so read it once the refresh you
 * care about has completed (see frame_done()) and not later. It is a plain
 * uint32_t and will eventually wrap; that is ~11k full-screen refreshes and it
 * is a diagnostic counter, so no saturation is attempted. */
uint32_t lvgl_mipi_panel_flushed_px();

/* --- v4: double-buffered create ------------------------------------------
 * Two framebuffers; LVGL renders off-screen and the LCDIFv2 flips at vsync.
 * LVGL ITSELF keeps the buffers coherent (refr_sync_areas, lv_refr.c:647)
 * -- this binding only supplies the second buffer and the flip.
 *
 * THE INVARIANT: LVGL never renders into a buffer the panel is scanning.
 * It holds because rendering waits on flush_wait_cb, and flush_wait_cb
 * returns only after the vsync at which the pending flip latched.
 *
 * BUFFER ORDER IS LOAD-BEARING: LVGL renders into buf1 FIRST
 * (lv_display.c:449), and the panel is scanning display.framebuffer() when
 * this is called -- so buf1 is the freshly allocated ALT buffer and buf2 is
 * the scanout one.  Swap them and the first frame renders into live scanout.
 *
 * Allocates the alt framebuffer itself (lcdifv2AllocAltFramebuffer) and
 * asserts on failure -- there is no silent single-buffer fallback.
 * The v1 accessors (frame_done, flushed_px) work for this path too. */
lv_display_t *lvgl_mipi_panel_create_db(DisplayClass &display);

/* Diagnostics since create_db(), reset by it:
 *   flips           shadow-load pulses issued (one per full refresh)
 *   vsyncs          vsync events consumed waiting for flips to land
 *   vsync_timeouts  waits that gave up (2 frame periods) -- 0 on any healthy
 *                   run; a non-zero value means vsync is dead and the gate
 *                   must go red on it rather than hang. */
uint32_t lvgl_mipi_panel_flips();
uint32_t lvgl_mipi_panel_vsyncs();
uint32_t lvgl_mipi_panel_vsync_timeouts();

/* The buffer the panel is scanning after the last landed flip (the pointer
 * handed to the last lcdifv2FlipTo whose vsync was consumed), or nullptr
 * before the first flip lands.  The flip test checksums exactly this. */
const uint16_t *lvgl_mipi_panel_scanned_fb();

/* Block until the pending flip has landed (bounded: two frame periods).
 * No-op when none is pending.  flush_wait_cb calls this; the flip test also
 * calls it directly so it can read the panel tap between a landed flip and
 * the next render. */
void lvgl_mipi_panel_flip_sync();
