/* lvgl_pxp_draw_census.cpp - the v9 counter-only draw unit (see the header).
 *
 * SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "lvgl_pxp_draw_census.h"
#include "lvgl.h"
#include "lvgl_private.h"   /* lv_draw_unit_t / lv_draw_task_t internals */

/* --- the classification grid ------------------------------------------------
 * Types: the lv_draw_task_type_t enum through MASK_BITMAP (0..12), then one
 * OTHER slot for anything beyond (VECTOR/3D when configured in, or a future
 * type).  Buckets: task area vs the sorted v9 bench-ladder areas; the last
 * bucket catches anything larger than full-panel. */
static const uint32_t BUCKET_EDGE[] = { 1024u, 4800u, 38400u, 57600u,
                                        120000u, 921600u };
static const size_t   N_EDGES  = sizeof(BUCKET_EDGE) / sizeof(BUCKET_EDGE[0]);
static const size_t   N_BUCKET = N_EDGES + 1;          /* + "over" */

static const char *const TYPE_NAME[] = {
    "NONE", "FILL", "BORDER", "BOX_SHADOW", "LETTER", "LABEL", "IMAGE",
    "LAYER", "LINE", "ARC", "TRIANGLE", "MASK_RECTANGLE", "MASK_BITMAP",
    "OTHER",
};
static const size_t N_TYPE = sizeof(TYPE_NAME) / sizeof(TYPE_NAME[0]);
static const size_t OTHER_SLOT = N_TYPE - 1;

static uint32_t s_count[N_TYPE][N_BUCKET];
static uint32_t s_tasks     = 0;
static uint64_t s_eval_cyc  = 0;

/* The unit struct: nothing beyond the base -- state is file-static, the
 * install is once-per-boot like the rest of the port. */
struct census_unit_t {
    lv_draw_unit_t base_unit;
};

/* evaluate_cb -- count, never claim.  The dispatcher (lv_draw.c,
 * lv_draw_finalize_task_creation) DISCARDS this return value; a unit "takes"
 * a task only by writing task->preferred_draw_unit_id (+ preference_score),
 * which this function deliberately never does.  Return 0 to match the SW
 * unit's convention for "evaluated". */
static int32_t census_evaluate(lv_draw_unit_t *u, lv_draw_task_t *task)
{
    (void)u;
    const uint32_t t0 = ARM_DWT_CYCCNT;

    size_t ti = (size_t)task->type;
    if (ti >= OTHER_SLOT) ti = OTHER_SLOT;

    const uint32_t area = (uint32_t)lv_area_get_size(&task->area);
    size_t bi = 0;
    while (bi < N_EDGES && area > BUCKET_EDGE[bi]) bi++;

    s_count[ti][bi]++;
    s_tasks++;

    s_eval_cyc += (uint32_t)(ARM_DWT_CYCCNT - t0);
    return 0;
}

/* dispatch_cb -- always idle.  lv_draw_dispatch_layer calls every unit's
 * dispatch_cb unconditionally (no NULL check) and treats any return other
 * than LV_DRAW_UNIT_IDLE as "a task was dispatched", which would make the
 * refresh loop spin waiting on work that does not exist.  The census never
 * has work: LV_DRAW_UNIT_IDLE, always. */
static int32_t census_dispatch(lv_draw_unit_t *u, lv_layer_t *layer)
{
    (void)u; (void)layer;
    return LV_DRAW_UNIT_IDLE;
}

void lvgl_pxp_draw_census_install(void)
{
    census_unit_t *unit =
        (census_unit_t *)lv_draw_create_unit(sizeof(census_unit_t));
    unit->base_unit.name        = "CENSUS";
    unit->base_unit.evaluate_cb = census_evaluate;
    unit->base_unit.dispatch_cb = census_dispatch;
    /* delete_cb / wait_for_finish_cb / event_cb stay NULL: every caller in
     * lv_draw.c NULL-checks them (only dispatch_cb is called bare). */
}

void lvgl_pxp_draw_census_print(void)
{
    for (size_t ti = 0; ti < N_TYPE; ti++) {
        for (size_t bi = 0; bi < N_BUCKET; bi++) {
            if (s_count[ti][bi] == 0) continue;
            if (bi < N_EDGES) {
                Serial1.printf("CENSUS type=%s bucket=%lu n=%lu\n",
                               TYPE_NAME[ti],
                               (unsigned long)BUCKET_EDGE[bi],
                               (unsigned long)s_count[ti][bi]);
            } else {
                Serial1.printf("CENSUS type=%s bucket=over n=%lu\n",
                               TYPE_NAME[ti],
                               (unsigned long)s_count[ti][bi]);
            }
        }
    }
    Serial1.printf("CENSUS_TASKS=%lu\n", (unsigned long)s_tasks);
    /* Mean evaluate cost in ns: cycles/task at F_CPU_ACTUAL.  64-bit
     * throughout -- 120 frames of full-panel scenes stay far below any
     * overflow, but the arithmetic shouldn't be the thing that caps it. */
    uint64_t mean_ns = 0;
    if (s_tasks > 0 && F_CPU_ACTUAL > 0) {
        mean_ns = (s_eval_cyc * 1000000000ull) / F_CPU_ACTUAL / s_tasks;
    }
    Serial1.printf("CENSUS_EVAL_NS=%lu\n", (unsigned long)mean_ns);
}
