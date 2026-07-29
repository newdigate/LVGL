/* lvgl_gt911_indev.cpp - see lvgl_gt911_indev.h.
 * SPDX-License-Identifier: MIT */
#include <Arduino.h>
#include "lvgl_gt911_indev.h"

static GT911   *s_touch = nullptr;
static int32_t  s_hor = 0, s_ver = 0;     /* display resolution, cached at create() */

/* Latched pointer state -- what Idle and Failed forward unchanged. */
static bool       s_pressed = false;
static lv_point_t s_point   = {0, 0};

/* Primary-contact tracking (header: "PRIMARY-CONTACT POLICY"). */
static bool    s_have_primary = false;
static uint8_t s_primary_id   = 0;
static bool    s_wait_clear   = false;   /* primary lifted while others remained */

static uint32_t s_idle_polls = 0;
static uint32_t s_poll_fails = 0;
static uint32_t s_buffers    = 0;

/* Not volatile, same rationale as the display bindings: with LV_USE_OS ==
 * LV_OS_NONE everything here runs from lv_timer_handler() in one thread. */

static void gt911_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    (void)indev;
    TouchPoint pts[5];
    uint8_t n = 0;

    /* The four-way branch IS the design (spec table); do not merge arms.
     * Idle and Failed both fall through to the latched state below. */
    switch (s_touch->read(pts, 5, &n)) {
    case GT911::Poll::Idle:
        s_idle_polls++;
        break;
    case GT911::Poll::Failed:
        /* A bus glitch is not a touch-up.  The latched state stands. */
        s_poll_fails++;
        break;
    case GT911::Poll::Released:
        s_buffers++;
        s_pressed      = false;
        s_have_primary = false;
        s_wait_clear   = false;          /* the panel is clear: re-arm adoption */
        break;
    case GT911::Poll::Contacts:
        s_buffers++;
        if (s_have_primary) {
            bool found = false;
            for (uint8_t i = 0; i < n; i++) {
                if (pts[i].id == s_primary_id) {
                    /* Scale by what the part REPORTED; identity on this panel,
                     * but never assumed (v2 spec 5.4 discipline). */
                    s_point.x = (int32_t)((uint32_t)pts[i].x * (uint32_t)s_hor
                                          / s_touch->resolutionX());
                    s_point.y = (int32_t)((uint32_t)pts[i].y * (uint32_t)s_ver
                                          / s_touch->resolutionY());
                    found = true;
                    break;
                }
            }
            if (!found) {
                /* The primary lifted while other contacts remain: release, and
                 * adopt nothing until the panel reports clear.  This is the arm
                 * phase 3b of the QEMU script exists to reach. */
                s_pressed      = false;
                s_have_primary = false;
                s_wait_clear   = true;
            }
        } else if (!s_wait_clear && n > 0) {
            /* First contact on a clear panel: adopt as primary, by track id. */
            s_primary_id   = pts[0].id;
            s_have_primary = true;
            s_pressed      = true;
            s_point.x = (int32_t)((uint32_t)pts[0].x * (uint32_t)s_hor
                                  / s_touch->resolutionX());
            s_point.y = (int32_t)((uint32_t)pts[0].y * (uint32_t)s_ver
                                  / s_touch->resolutionY());
        }
        /* s_wait_clear && Contacts: surviving fingers are ignored entirely. */
        break;
    }

    data->state = s_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point = s_point;
    /* continue_reading stays false: the GT911 publishes nothing new until the
     * read above acknowledged it, so there is never a queue to drain. */
}

lv_indev_t *lvgl_gt911_indev_create(lv_display_t *disp, GT911 &touch)
{
    /* Both preconditions from the header, enforced loudly.  LV_ASSERT_HANDLER
     * prints LVGL_ASSERT! on Serial1 first (lvgl_rt1176_assert.h). */
    LV_ASSERT(touch.resolutionX() != 0 && touch.resolutionY() != 0);
    LV_ASSERT(lv_display_get_rotation(disp) == LV_DISPLAY_ROTATION_0);

    s_touch = &touch;
    s_hor = lv_display_get_horizontal_resolution(disp);
    s_ver = lv_display_get_vertical_resolution(disp);
    s_pressed = false;
    s_point.x = 0; s_point.y = 0;
    s_have_primary = false;
    s_primary_id   = 0;
    s_wait_clear   = false;
    s_idle_polls = s_poll_fails = s_buffers = 0;

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, gt911_read_cb);
    lv_indev_set_display(indev, disp);
    /* 10 ms, per-indev -- see the header's READ TIMING note. */
    lv_timer_set_period(lv_indev_get_read_timer(indev), 10);
    return indev;
}

uint32_t lvgl_gt911_idle_polls() { return s_idle_polls; }
uint32_t lvgl_gt911_poll_fails() { return s_poll_fails; }
uint32_t lvgl_gt911_buffers()    { return s_buffers; }
