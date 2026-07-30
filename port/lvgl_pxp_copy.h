/* lvgl_pxp_copy.h - PXP-backed lv_draw_buf copy handler for the i.MX RT1176.
 *
 * C++ ONLY, like the rest of the port (lvgl_rt1176.h's rationale).
 * Deliberately NOT part of any display binding: nothing gains a PXP
 * dependency it didn't ask for.  Compiled only by examples that also
 * import_evkb_library(PXP).
 *
 * SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include "lvgl.h"

/* Install the PXP-backed buf_copy handler on LVGL's GLOBAL draw-buf handlers
 * (lv_draw_buf_get_handlers()).  Saves the default CPU copy and CHAINS to it:
 * anything that is not the exact accelerated shape falls through -- never a
 * silent wrong copy.  The accelerated shape is ALL of:
 *   - RGB565 on both sides, dest_area and src_area the same size,
 *   - copy height >= 2 ROWS -- the LOAD-BEARING check.  The bench
 *     (examples/display/lvgl_pxp_copy_bench/transcript_hw_evkb.txt, ANALYSIS
 *     point 3) measured the PXP winning 13 of 14 cases, 5.4x-21x; its one
 *     loss was the single-row case (719x1: 70 us CPU vs 105 us PXP).  The
 *     discriminator is height, not area: 1x1280 (1280 one-pixel rows) still
 *     wins 2x on the PXP.
 *   - copy area >= threshold_px (belt-and-braces floor under the height rule;
 *     cite the bench at the call site),
 *   - both strides sane (>= the buffer's row bytes), both data pointers
 *     non-null, the areas inside their buffers.
 * There is deliberately NO reachability pre-check here: the framebuffer
 * allocator's extmem/SDRAM buffers qualify, and a genuinely unreachable
 * surface comes back from the PXP as an error, which chains to the CPU
 * default anyway -- degraded loud (a counter), correct always.
 * Synchronous: PXP.blit() includes the completion wait.  SINGLE PXP OWNER:
 * an example that installs this must not run other PXP work concurrently
 * (none does -- fillScreen runs before LVGL starts).  Install AFTER lv_init
 * (lvgl_rt1176_begin), BEFORE the display binding allocates draw buffers. */
void lvgl_pxp_copy_install(uint32_t threshold_px);

/* Diagnostics since install: copies taken by the PXP vs fallen through to the
 * saved CPU default.  Adopting gates assert PXP_COPIES>0 (the IDLE_POLLS
 * idiom) so the handler being silently dead cannot pass as adopted. */
uint32_t lvgl_pxp_copies();
uint32_t lvgl_pxp_copy_fallbacks();
