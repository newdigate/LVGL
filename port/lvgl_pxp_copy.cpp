/* lvgl_pxp_copy.cpp - PXP-backed lv_draw_buf copy handler (see the header).
 *
 * SPDX-License-Identifier: MIT */
#include "lvgl_pxp_copy.h"
#include "lvgl_private.h"   /* _lv_draw_buf_handlers_t: buf_copy_cb lives here */
#include "PXP.h"

static lv_draw_buf_copy_cb_t s_default_copy = nullptr;
static uint32_t s_pxp_copies   = 0;
static uint32_t s_fallbacks    = 0;
static uint32_t s_pxp_errors   = 0;   /* subset of fallbacks: the PXP itself failed */
static uint32_t s_threshold_px = 0;

/* True when `area` lies fully inside `buf` and the buffer's stride can carry
 * it.  LVGL's refr sync path always passes intersected in-bounds areas; this
 * guard exists so a caller that doesn't cannot make the PXP write outside the
 * buffer -- it falls through to the default instead. */
static bool area_fits(const lv_draw_buf_t *buf, const lv_area_t *area)
{
    if (area->x1 < 0 || area->y1 < 0) return false;
    if (area->x2 >= (int32_t)buf->header.w) return false;
    if (area->y2 >= (int32_t)buf->header.h) return false;
    /* Stride sane: at least the buffer's own RGB565 row bytes.  (RGB565 is
     * already established by the caller's format check.) */
    return buf->header.stride >= buf->header.w * 2u;
}

/* Signature: lv_draw_buf_copy_cb_t, lv_draw_buf.h:82. */
static void pxp_copy_cb(lv_draw_buf_t *dest, const lv_area_t *dest_area,
                        const lv_draw_buf_t *src, const lv_area_t *src_area)
{
    /* A NULL area legally means "the whole buffer" to the default copy
     * (buf_copy, lv_draw_buf.c); let the default keep owning that spelling. */
    if (!dest || !src || !dest_area || !src_area) {
        s_fallbacks++;
        s_default_copy(dest, dest_area, src, src_area);
        return;
    }

    const int32_t w = lv_area_get_width(dest_area);
    const int32_t h = lv_area_get_height(dest_area);

    /* Accelerated shape -- ALL required; anything else chains to the saved
     * default.  The height >= 2 check is the load-bearing one: the bench's
     * single-row case (719x1) is the CPU's ONE win (transcript ANALYSIS
     * point 3); every multi-row case measured 2x-21x for the PXP. */
    const bool shape_ok =
        dest->data && src->data &&
        dest->header.cf == LV_COLOR_FORMAT_RGB565 &&
        src->header.cf  == LV_COLOR_FORMAT_RGB565 &&
        w == lv_area_get_width(src_area) &&
        h == lv_area_get_height(src_area) &&
        h >= 2 &&
        (uint32_t)w * (uint32_t)h >= s_threshold_px &&
        area_fits(dest, dest_area) && area_fits(src, src_area);

    if (!shape_ok) {
        s_fallbacks++;
        s_default_copy(dest, dest_area, src, src_area);
        return;
    }

    /* Offset-base sub-rect surfaces, exactly as the bench proved: the surface
     * base is the area's first pixel, the pitch is the FULL buffer stride, the
     * dims are the area's.  (PXPSurface has no origin field.) */
    uint8_t *sp = src->data
                + (uint32_t)src_area->y1 * src->header.stride
                + (uint32_t)src_area->x1 * 2u;
    uint8_t *dp = dest->data
                + (uint32_t)dest_area->y1 * dest->header.stride
                + (uint32_t)dest_area->x1 * 2u;
    const PXPSurface ssrc(sp, (uint16_t)w, (uint16_t)h, PXP_RGB565,
                          (uint16_t)src->header.stride);
    const PXPSurface sdst(dp, (uint16_t)w, (uint16_t)h, PXP_RGB565,
                          (uint16_t)dest->header.stride);

    /* Synchronous: blit() is op().source().output().run(), and run() includes
     * the bounded completion wait (PXP.cpp).  On ANY error -- not begun,
     * unreachable, config, AXI, timeout -- run the default copy anyway:
     * degraded loud (the counter), correct always (the CPU copy runs). */
    if (PXP.blit(ssrc, sdst) != PXP_OK) {
        /* Counted APART from shape fallbacks: a dying PXP should be loud by
         * name, not a drifting ratio -- adopting gates pin PXP_ERRORS=0.  On
         * PXP_ERR_TIMEOUT specifically the library returns with the op still
         * enabled; a pathologically late completion could in principle write
         * after the CPU copy below, but a blit that missed the 100 ms cap is
         * hung in practice and every later call lands here as PXP_ERR_BUSY:
         * a permanent, visible CPU fallback.  (v6 final review, items 3-4.) */
        s_pxp_errors++;
        s_fallbacks++;
        s_default_copy(dest, dest_area, src, src_area);
        return;
    }
    s_pxp_copies++;
}

void lvgl_pxp_copy_install(uint32_t threshold_px)
{
    lv_draw_buf_handlers_t *h = lv_draw_buf_get_handlers();
    /* Idempotent under re-install: never save our own wrapper as "default"
     * (that would make the fallback path recurse). */
    if (h->buf_copy_cb != pxp_copy_cb) s_default_copy = h->buf_copy_cb;
    s_threshold_px = threshold_px;
    /* Idempotent bring-up, same rationale as Display::fillScreen()
     * (MipiDisplay Display.cpp): if begin() fails, the handler still installs
     * and every copy falls back with PXP_ERR_NOT_BEGUN -- visible in the
     * fallback counter, never a wrong copy. */
    (void)PXP.begin();
    h->buf_copy_cb = pxp_copy_cb;
}

uint32_t lvgl_pxp_copies()        { return s_pxp_copies; }
uint32_t lvgl_pxp_copy_fallbacks() { return s_fallbacks; }
uint32_t lvgl_pxp_copy_errors()    { return s_pxp_errors; }
