/* lvgl_rt1176_assert.h - LV_ASSERT_HANDLER hook for the i.MX RT1176 port.
 *
 * Pulled in by lv_conf.h via LV_ASSERT_HANDLER_INCLUDE, so it is included from
 * the vendored C tree and must stay C-compatible. Kept separate from
 * lvgl_rt1176.h to avoid a circular include (lvgl_rt1176.h -> lvgl.h ->
 * lv_conf.h -> here).
 *
 * WHY IT EXISTS: with LV_USE_LOG 0, LVGL's stock handler is a bare `while(1);`.
 * In a project whose gates are UART tokens, an assertion (e.g. the 1 MB pool
 * exhausting) would then be an infinite hang with ZERO output -- the gate goes
 * red having told you nothing. This prints an identifiable token first.
 *
 * SPDX-License-Identifier: MIT */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Prints "LVGL_ASSERT!" to Serial1 (flushed) and returns. The caller -- the
 * LV_ASSERT_HANDLER macro in lv_conf.h -- halts immediately afterwards. */
void lvgl_rt1176_assert_report(void);

#ifdef __cplusplus
}
#endif
