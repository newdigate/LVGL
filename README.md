# LVGL for i.MX RT1176

Vendored [LVGL](https://lvgl.io) v9.4.0 (MIT) plus an RT1176 port layer, packaged
as an Arduino-style library for the
[rt1176-evkb](https://github.com/newdigate/rt1176-evkb) core.

## Layout

- `lvgl/` — the vendored upstream tree, taken from the NXP MCUXpresso SDK.
  **Never hand-edit anything in here.** Several directories are pruned on
  vendoring — three for licensing reasons, six for size; see
  [`VENDORING.md`](VENDORING.md) for exactly what was removed, why, the pinned
  upstream provenance, and how to re-vendor a newer LVGL.
- `port/` — the RT1176 port. Present today:
  - `lv_conf.h` — derived from `lvgl/lv_conf_template.h`. Its banner lists every
    setting changed from the template, and the four licence-critical
    `MUST STAY 0` options are commented at their own definitions. **Re-vendoring
    means re-copying the template and re-applying those edits**, not overwriting
    this file.
  - `lvgl_rt1176.{h,cpp}` — `lv_init()` + the `millis()`-based tick callback, the
    `lv_timer_handler()` pump, and the FNV-1a-32 checksum accumulator the QEMU
    gates use as their pixel oracle.
  - `lvgl_rt1176_assert.h` — the `LV_ASSERT_HANDLER` hook, so an LVGL assertion
    emits a UART token instead of hanging silently.

  Still to come: the display bindings for the Raspberry Pi 7" MIPI-DSI panel and
  the ILI9341 SPI panel. Those are deliberately *not* compiled into the `LVGL`
  target — each example compiles the one binding it needs, so an ILI9341 sketch
  never pulls in RPiDisplay.

## Consuming it

The evkb project pulls this repo in via `import_evkb_lvgl()` from its
`evkb.cmake`, which builds `lvgl/src/**/*.c` (C only — `src/libs/thorvg/` is C++
and needs a `config.h` absent from a fresh clone) plus `port/lvgl_rt1176.cpp`
into a static `LVGL` target. Because it is a plain CMake target rather than a
`teensy_add_library()` one, an example links it directly, following the
CMSIS-DSP precedent:

```cmake
import_evkb_lvgl()
teensy_add_executable(my_sketch my_sketch.cpp)
teensy_target_link_libraries(my_sketch cores)
target_link_libraries(my_sketch.elf LVGL stdc++)
```

Library resolution is local-first: a `~/Development/LVGL` checkout wins over the
pinned GitHub fetch, so uncommitted edits here are picked up by example builds
automatically.

The 1 MB `LV_MEM_SIZE` pool does not fit the RT1176's 256 KB DTCM, so
`LV_ATTRIBUTE_LARGE_RAM_ARRAY` places it in the board's 64 MB SDRAM via the
core's `.externalram` section — linker-placed, so no hard-coded address.

The worked example is `examples/display/lvgl_smoke_test` in the evkb repo.

## Licence

LVGL is MIT — see [`LICENSE`](LICENSE). This tree is deliberately MIT/BSD-only
and is swept by the evkb project's `tools/license-audit.sh`; copyleft sources are
pruned rather than allowlisted.
