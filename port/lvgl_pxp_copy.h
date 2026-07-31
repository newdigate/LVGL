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
 *   - RGB565 or XRGB8888, source and dest the same format, dest_area and
 *     src_area the same size,
 *   - copy height >= 2 ROWS -- the LOAD-BEARING check.  The v7 bench
 *     (examples/display/lvgl_pxp_copy_bench/transcript_hw_evkb.txt, ANALYSIS
 *     point 1) holds this for BOTH formats: every multi-row case wins the
 *     PXP, 3.4x-26.5x at XRGB8888 (~13.4x for the bulk copies) and 2x-21x at
 *     RGB565; the single-row case (719x1) is the CPU's one win at RGB565
 *     (70 vs 105 us) but only a dead tie at XRGB8888 (135 vs 135 us).  The
 *     discriminator is height, not area: 1x1280 (1280 one-pixel rows) still
 *     wins 2x on the PXP.
 *   - copy area >= threshold_px (belt-and-braces floor under the height rule;
 *     cite the bench at the call site),
 *   - both strides sane (>= the buffer's row bytes at the format's pixel
 *     size), both data pointers non-null, the areas inside their buffers.
 * MEASURED 32-BIT CONTRACT (same transcript): at XRGB8888 the PXP copies the
 * RGB bytes exactly but writes the X byte as its computed alpha, which is 0
 * with the alpha engine unconfigured -- NOT a byte-preserving copy of that
 * byte.  Harmless for XRGB (LVGL, LCDIFv2 and the 24-bit DSI wire all ignore
 * it, and QEMU's model writes the same 0), but the reason ARGB8888 (where
 * that byte is meaningful) is deliberately excluded above.
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
/* The subset of fallbacks caused by a PXP ERROR (vs a shape that was never
 * eligible).  0 on any healthy run; adopting gates pin it so a dying PXP is
 * loud by name rather than a drifting fallback ratio. */
uint32_t lvgl_pxp_copy_errors();
