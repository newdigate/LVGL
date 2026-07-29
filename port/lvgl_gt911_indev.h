/* lvgl_gt911_indev.h - LVGL pointer-input binding for the GT911 capacitive
 * touch controller (i.MX RT1176, TouchPanel library).  The first input
 * binding in this port; the FT5406 gets a sibling file when it arrives, and
 * any shared base is extracted then, from two real implementations.
 * SPDX-License-Identifier: MIT */
#pragma once
#include "gt911.h"
#include "lvgl_rt1176.h"

/* Creates an LV_INDEV_TYPE_POINTER over `touch` and binds it to `disp`.
 *
 * PRECONDITIONS (asserted, not merely documented):
 *   - touch.begin() has SUCCEEDED.  Checked via resolutionX/Y() != 0, which
 *     the driver guarantees exactly distinguishes a successful begin().
 *     read() before a successful begin() touches no bus and returns Failed,
 *     which this binding forwards as "no change" -- safe, but a permanently
 *     dead pointer, so it is refused loudly here instead.
 *   - lv_display_get_rotation(disp) == LV_DISPLAY_ROTATION_0.  In this port's
 *     configuration LVGL rotation CANNOT work (direct render + no matrix
 *     transform: LV_DRAW_TRANSFORM_USE_MATRIX is 0), and setting it would skew
 *     the renderer against the live scanout stride while this binding's
 *     mapping pointed somewhere else again.  QEMU can never catch either
 *     (model and firmware share the orientation assumption), so the failure
 *     is made loud at create time.  The day landscape is wanted, that is a
 *     display-binding milestone (v4+), not an edit to this assert.
 *
 * READ TIMING: sets this indev's read timer to 10 ms (a real GT911 publishes
 * at ~100 Hz).  Per-indev via lv_indev_get_read_timer() -- lv_conf.h's 33 ms
 * default is untouched, so no other example's timing or golden can shift.
 * At 33 ms the QEMU model (re-armed 20 ms after each ack) would have a fresh
 * buffer at EVERY poll and the Idle latch below would be dead code in the
 * gate; at 10 ms idle polls occur mid-drag, which is what makes the touch
 * gate's drag assertion able to fail.  See the spec, "the read period is a
 * verification decision".
 *
 * STATE MODEL (the spec's table; do not merge branches):
 *   Contacts  primary-contact policy (below), LV_INDEV_STATE_PRESSED
 *   Released  LV_INDEV_STATE_RELEASED, primary cleared
 *   Idle      the LATCHED previous state, unchanged -- "nothing new since the
 *             part was acknowledged" is not an event, and a poll loop outruns
 *             the part's publish rate, so Idle is the COMMON case
 *   Failed    ALSO the latched state -- a bus glitch is not a touch-up.
 *             (NXP's own binding forwards both Idle and Failed as RELEASED:
 *             fsl_gt911.c:261,290 + lvgl_support.c:575-580.  That is the bug
 *             this project has now recorded four instances of.)
 *
 * PRIMARY-CONTACT POLICY (one pointer from up to five contacts): the first
 * contact on an otherwise-clear panel becomes the primary, identified by its
 * TRACK ID (the driver sorts by id, so an array slot silently changes meaning
 * when a lower id arrives).  While present, its coordinates are the pointer;
 * other contacts are ignored.  When it disappears the pointer RELEASES, and
 * no new primary is adopted until a poll reports zero contacts.  This makes
 * the pointer-teleports-between-fingers artefact unrepresentable, at the
 * documented cost that after a multi-finger touch, ALL fingers must lift
 * before the next touch registers.
 *
 * COORDINATES are scaled by the resolution the part REPORTED
 * (resolutionX/Y()), never by an assumed 720x1280.  Scale only -- no swap, no
 * mirror: the identity mapping v2 proved with a finger on this panel.
 *
 * NOT REENTRANT, single instance: one GT911 on this board, module state like
 * the display bindings.  read_cb runs from lv_timer_handler() only. */
lv_indev_t *lvgl_gt911_indev_create(lv_display_t *disp, GT911 &touch);

/* Diagnostics since create() -- reset by create(), same style as the display
 * bindings' counters.  idle_polls is the touch gate's proof its Idle-latch
 * assertion is not vacuous (assert > 0); poll_fails must be 0 on a clean run
 * (QEMU cannot fault I2C, so any non-zero there is unmodelled behaviour);
 * buffers counts fresh buffers consumed (Contacts + Released) -- against a
 * scripted QEMU run it is exact and pins that every instant was consumed. */
uint32_t lvgl_gt911_idle_polls();
uint32_t lvgl_gt911_poll_fails();
uint32_t lvgl_gt911_buffers();
