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
- `port/` — the RT1176 port: `lv_conf.h`, the `millis()`-based tick, and display
  bindings for the Raspberry Pi 7" MIPI-DSI panel and the ILI9341 SPI panel.
  This directory is currently empty — it is filled in by later bring-up tasks.

## Consuming it

*(Forward-looking — the integration below does not exist yet; a later bring-up
task adds it.)* The evkb project will pull this repo in via `import_evkb_lvgl()`
from its `evkb.cmake`. Library resolution is local-first: a `~/Development/LVGL`
checkout wins over the pinned GitHub fetch, so uncommitted edits here are picked
up by example builds automatically.

## Licence

LVGL is MIT — see [`LICENSE`](LICENSE). This tree is deliberately MIT/BSD-only
and is swept by the evkb project's `tools/license-audit.sh`; copyleft sources are
pruned rather than allowlisted.
