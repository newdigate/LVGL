/* lvgl_rt1176.h - LVGL 9.4 port entry points for the i.MX RT1176.
 *
 * C++ ONLY. The implementation is C++ (it uses the Arduino core's Serial1), and
 * the declarations below are deliberately NOT wrapped in extern "C": nothing in
 * the vendored C tree calls them, only sketches and the C++ display bindings do.
 * The one symbol C code does reach -- the LV_ASSERT_HANDLER hook -- lives in
 * lvgl_rt1176_assert.h with its own extern "C" guard.
 *
 * SPDX-License-Identifier: MIT */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "lvgl.h"

/* lv_init() + lv_tick_set_cb(millis). Call once, before creating a display. */
void lvgl_rt1176_begin();

/* Drives LVGL's timers/rendering. Call from loop().
 * The ms-to-next-timer value lv_timer_handler() returns is DROPPED on purpose:
 * this port is bare-metal with a free-running loop() and nothing to sleep into,
 * so there is no idle path to feed it to. A caller that does want to idle (WFI,
 * a scheduler delay) should skip this wrapper and call lv_timer_handler()
 * directly so it can act on the return value. */
void lvgl_rt1176_loop();

/* --- FNV-1a-32 accumulator: the gate oracle -------------------------------
 * LVGL's software renderer is deterministic CPU work, so checksumming the
 * pixels it produces gives a green/red QEMU gate even for a panel QEMU does
 * not model (the ILI9341). Bindings feed it; examples read it.
 * It is an ACCUMULATOR because partial render mode delivers a frame as a
 * sequence of slices through flush_cb.
 *
 * CONTRACT (v1 deliberately has no context struct -- one display per example,
 * so there is exactly one feeder):
 *   - Exactly ONE feeder at a time. Two concurrent feeders interleave into one
 *     meaningless value with no diagnostic.
 *   - Call lvgl_sum_reset() IMMEDIATELY BEFORE the frame you intend to
 *     checksum -- not at startup, not once per session.
 *   - A forgotten reset fails SILENTLY: the checksum is simply a different
 *     number, indistinguishable from a legitimately different image. That is
 *     what lvgl_sum_bytes() is for -- assert the expected pixel count actually
 *     arrived, and "reset forgotten" / "flush_cb never ran" become
 *     distinguishable from "the image changed". A gate that checks only
 *     lvgl_sum_value() cannot tell those apart. */
void     lvgl_sum_reset();
void     lvgl_sum_feed(const void *data, size_t bytes);
uint32_t lvgl_sum_value();

/* Bytes fed since the last lvgl_sum_reset(). See the contract above. */
uint32_t lvgl_sum_bytes();
