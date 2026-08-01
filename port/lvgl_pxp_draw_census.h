/* lvgl_pxp_draw_census.h - v9 P1: a COUNTER-ONLY draw unit (the census).
 *
 * C++ ONLY, like the rest of the port (lvgl_rt1176.h's rationale).
 *
 * THE CONTRACT (the v9 census): this unit registers via lv_draw_create_unit
 * and observes every draw task the dispatcher finalizes -- type, area bucket
 * -- and NEVER takes one.  Provably inert:
 *
 *   - evaluate_cb counts and returns; it does NOT touch
 *     task->preferred_draw_unit_id or task->preference_score.  In this
 *     LVGL (9.4, lv_draw.c lv_draw_finalize_task_creation) "taking" a task
 *     is expressed ONLY by writing those two fields; the evaluate_cb return
 *     value itself is discarded by the dispatcher.  Touch neither and the
 *     task flows to whoever else wants it (the SW unit), exactly as if the
 *     census did not exist.
 *   - dispatch_cb always returns LV_DRAW_UNIT_IDLE ("I have nothing and
 *     will take nothing"), the convention lv_draw_dispatch_layer expects
 *     from an idle unit.  Any other return would make the refresh loop
 *     believe work was dispatched and spin.
 *   - Being installed DOES flip the dispatcher's unit count to 2, which
 *     moves the SW unit's task search from the single-unit linear path to
 *     lv_draw_get_next_available_task (lv_draw.c lv_draw_get_available_task
 *     branches on unit_cnt == 1).  Same tasks, same pixels -- the adopting
 *     examples' QEMU gates prove it: every pinned token and golden must be
 *     byte-identical with the census installed (the v9 inert-proof).
 *
 * It also accumulates its own per-task evaluate cost (DWT cycle counter):
 * the overhead constant the v9 P2 projection subtracts from any predicted
 * PXP win.
 *
 * Install AFTER lv_init (lvgl_rt1176_begin): the SW unit must already
 * exist.  lv_draw_create_unit PREPENDS, so the census's evaluate_cb runs
 * first -- irrelevant to the outcome (it writes nothing) but it means the
 * census sees every task even if a later unit claims it.
 *
 * SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>

/* Register the census unit.  Call once, after lv_init. */
void lvgl_pxp_draw_census_install(void);

/* Emit the histogram on Serial1:
 *   CENSUS type=<name> bucket=<edge> n=<count>   (nonzero cells only;
 *       bucket=<edge> means area <= edge px, edges 1024/4800/38400/57600/
 *       120000/921600 -- the v9 bench ladder areas -- and bucket=over
 *       means larger than the full-panel 921600)
 *   CENSUS_TASKS=<total observed>
 *   CENSUS_EVAL_NS=<mean evaluate_cb cost per task, ns>
 */
void lvgl_pxp_draw_census_print(void);
