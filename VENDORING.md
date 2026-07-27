# Vendoring LVGL

Upstream: LVGL **v9.4.0** (MIT), taken from the NXP MCUXpresso SDK at
`mcuxsdk/middleware/lvgl`.

Exact provenance, so this tree can be re-derived and byte-compared:

| | |
|---|---|
| MCUXpresso SDK release | `v26.06.00-LTS` (west manifest tag; `MCUX_VERSION` = 26.06.00) |
| `mcuxsdk` repo commit | `a910e7645d2d809a3431e1d5f42fca1cdeee69c9` |
| Source path within SDK | `middleware/lvgl` |
| LVGL version | 9.4.0 (`lv_version.h`: MAJOR 9, MINOR 4, PATCH 0) |

## Pruned on vendoring — LICENCE (do not restore)

Removed because of their licence or missing licence. Restoring any of these
breaks the MIT/BSD-only guarantee this tree exists to uphold.

- `lvgl/src/libs/vg_lite_driver/` — VeriSilicon VGLite kernel, dual-licensed
  (trips `tools/license-audit.sh` Part 1 on 5 files under `VGLiteKernel/`).
  RT1176 has no VGLite GPU, so it can never be linked here.
- `lvgl/src/libs/frogfs/` — MPL-2.0 (weak copyleft). Unused.
- `lvgl/libs/nema_gfx/` — 8 prebuilt static archives (2.6 MB) with **no licence
  text anywhere in the directory**. Two independent reasons to delete:
  1. They are opaque binaries, and the audit's Part 1 is a *source-header grep* —
     `grep -I` skips binary files entirely, so these would ship past the gate
     invisibly. In a tree whose premise is provable provenance, an unlicensed
     binary the firewall is structurally blind to is precisely what the firewall
     exists to stop.
  2. They are `cortex_m33` builds (`lib/core/cortex_m33_revC/`,
     `lib/core/cortex_m33_NemaPVG/`). RT1176 is Cortex-M7 + Cortex-M4, so they
     are unlinkable here by construction.

  Contrast `lvgl/src/libs/freetype/LiberationSans-Regular.ttf`, which is also a
  binary the grep cannot read but ships `LiberationSans-LICENSE.txt` right
  beside it — that one is fine and is retained.

### These prunes delete code that `src/` still references

The pruned directories are *not* unreferenced. Three files under `src/` include
headers that no longer exist, and the `nema_gfx` draw backend needs the archives
that no longer exist:

| Referencing file | Include | Guard |
|---|---|---|
| `src/libs/fsdrv/lv_fs_frogfs.c:13` | `../frogfs/include/frogfs/frogfs.h` | `#if LV_USE_FS_FROGFS` (line 11) |
| `src/draw/vg_lite/lv_draw_vg_lite_type.h:28` | `../../libs/vg_lite_driver/inc/vg_lite.h` | `#if LV_USE_DRAW_VG_LITE` |
| `src/draw/vg_lite/lv_vg_lite_utils.h:28` | `../../libs/vg_lite_driver/inc/vg_lite.h` | `#if LV_USE_DRAW_VG_LITE` |
| `src/draw/nema_gfx/*.c` | `nema_core.h`, `nema_vg.h`, … (build include path) | `#if LV_USE_NEMA_GFX` |

Every one is `#if`-guarded and every guard is off, so the build is correct today.
But that means **`LV_USE_DRAW_VG_LITE`, `LV_USE_FS_FROGFS` and `LV_USE_NEMA_GFX`
must all stay 0** — and not merely as a config preference. Turning any of them on
is not a config change; it is a missing-source build failure, because the code it
needs was deliberately deleted for licence reasons. Do not "fix" such a failure
by restoring the directory.

Note also that the audit's copyleft regex matches GNU GPL/LGPL only. It does
**not** flag MPL-2.0, and it cannot see inside binaries at all. The frogfs and
nema_gfx prunes are therefore policy that a human must re-apply — the gate will
not catch them for you on re-vendor.

## Pruned on vendoring — SIZE (do not restore)

No licence problem with any of these. They are removed purely because none is
firmware-relevant, and together they are ~140 MB / ~3200 files of upstream test
fixtures, documentation and host-side tooling:

- `lvgl/tests/`
- `lvgl/demos/`
- `lvgl/docs/`
- `lvgl/scripts/`
- `lvgl/examples/`
- `lvgl/env_support/`

## What is kept

Everything not listed above is retained as-is. That is `lvgl/src/` plus the
directories `lvgl/configs/`, `lvgl/xmls/`, `lvgl/zephyr/`, `lvgl/.github/` and
`lvgl/.devcontainer/`, plus all 22 files at the `lvgl/` root (which includes
`COPYRIGHTS.md`, `Kconfig`, `SBOM.spdx.json`, `SConscript`, `library.json`,
`library.properties`, `CMakePresets.json`, `idf_component.yml`, `lv_version.h.in`,
`lvgl.pc.in` and `README.md`, not only the headers the build uses). Note that
`lvgl/libs/` is gone entirely — `nema_gfx` was its only content.

No attempt is made to trim to a minimal set beyond the prunes above.

Of what is retained, the firmware build consumes only `lvgl/src/**.c` plus the
root headers (`lvgl.h`, `lvgl_private.h`, `lv_version.h`, and
`lv_conf_template.h`, which `port/lv_conf.h` is derived from). The rest is
retained for reference and for upstream fidelity, not because it is built.

## Re-vendoring procedure

1. **Replace**, do not merge:

   ```sh
   rm -rf lvgl/
   cp -R <mcuxsdk>/middleware/lvgl/. lvgl/
   rm -rf lvgl/.git
   ```

   Copying over the existing directory would leave upstream-deleted files
   behind, silently drifting this tree from the release it claims to be.
2. Re-apply ALL deletions above — the licence prunes *and* the size prunes.
3. Update the provenance table at the top with the new SDK tag, commit and LVGL
   version.
4. Run `rt1176-evkb/tools/license-audit.sh` — it MUST pass with no new `ALLOW`
   entries. If new copyleft files appear, prune them; do not allowlist them.
   Remember the gate cannot see MPL or binaries: also re-check by hand for new
   prebuilt archives and for licence-less binary blobs.
5. Confirm `LV_USE_DRAW_VG_LITE`, `LV_USE_FS_FROGFS` and `LV_USE_NEMA_GFX` are
   still 0 in `port/lv_conf.h`.
6. *(Forward-looking — not yet in place.)* Once the example gates exist, re-record
   their golden `LVGL_SUM` values; a renderer or font change legitimately changes
   them. See each `run_qemu.sh`.
