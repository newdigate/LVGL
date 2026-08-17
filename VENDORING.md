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
  ~~RT1176 has no VGLite GPU, so it can never be linked here.~~

  > ★ **CORRECTION 2026-08-16 — "RT1176 has no VGLite GPU" was FALSE.** The
  > RT1176 has a Vivante **GC355** GPU2D (`0x4180_0000`, IRQ 60,
  > `GPU2D_CLK_ROOT`), and it has since been made to render: the evkb tree's
  > `display/vglite_probe` reads chip ID `0x355` off the silicon and draws to
  > the RK055 panel.
  >
  > **The prune stays, and the licence reason above is the only reason it ever
  > needed.** The dual-licensed text is what disqualifies *this copy*; NXP ships
  > the same driver MIT-only, and that is the one the evkb tree vendors (its own
  > `VGLite` sibling repo, with its own `VENDORING.md`). Enabling the GPU
  > required un-pruning nothing.
  >
  > Worth stating plainly because the wrong premise was load-bearing elsewhere:
  > it was reused to argue GPU acceleration was impossible on this board, which
  > cost real time. A claim can be false while the conclusion it was offered for
  > is right — check which one you are relying on.
- `lvgl/src/libs/frogfs/` — MPL-2.0 (weak copyleft). Unused. Its licence header
  is now detected by the gate: `tools/license-audit.sh` matches MPL as well as
  GNU GPL/LGPL, and treats it as a failure.
- `lvgl/libs/nema_gfx/` — 8 prebuilt static archives (2.6 MB) with **no licence
  text anywhere in the directory**. Two independent reasons to delete:
  1. They are opaque binaries, and the audit's Part 1 is a *source-header grep* —
     `grep -I` skips binary files entirely, so these would ship past the gate
     invisibly. In a tree whose premise is provable provenance, an unlicensed
     binary the firewall is structurally blind to is precisely what the firewall
     exists to stop. **The gate now closes this**: Part 1 additionally requires
     every git-tracked `.a`/`.o`/`.so`/`.dylib`/`.lib` to carry licence text in
     its own directory or one level up, and these archives carry none, so
     re-vendoring them fails the audit with `UNLICENSED BINARY`.
  2. They are `cortex_m33` builds (`lib/core/cortex_m33_revC/`,
     `lib/core/cortex_m33_NemaPVG/`). RT1176 is Cortex-M7 + Cortex-M4, so they
     are unlinkable here by construction.

  Contrast `lvgl/src/libs/freetype/LiberationSans-Regular.ttf`, which is also a
  binary the grep cannot read but ships `LiberationSans-LICENSE.txt` right
  beside it — that one is fine and is retained. The gate encodes that same
  distinction: the rule is *binaries without provenance*, not *no binaries*.
  Note it deliberately stops at one level up rather than walking to the repo
  root — `lvgl/LICENCE.txt` is an ancestor of `lvgl/libs/nema_gfx/`, and a
  root-to-leaf walk would have excused the very archives that motivated the
  check.

### These prunes delete code that `src/` still references

The pruned directories are *not* unreferenced. Three files under `src/` include
headers that no longer exist, and the `nema_gfx` draw backend needs the archives
that no longer exist:

| Referencing file | Include | Guard |
|---|---|---|
| `src/libs/fsdrv/lv_fs_frogfs.c:13` | `../frogfs/include/frogfs/frogfs.h` | `#if LV_USE_FS_FROGFS` (line 11) |
| `src/draw/vg_lite/lv_draw_vg_lite_type.h:28` | `../../libs/vg_lite_driver/inc/vg_lite.h` | `#if LV_USE_DRAW_VG_LITE` **and** `LV_USE_VG_LITE_DRIVER` |
| `src/draw/vg_lite/lv_vg_lite_utils.h:28` | `../../libs/vg_lite_driver/inc/vg_lite.h` | `#if LV_USE_DRAW_VG_LITE` **and** `LV_USE_VG_LITE_DRIVER` |
| `src/draw/nema_gfx/*.c` | `nema_core.h`, `nema_vg.h`, … (build include path) | `#if LV_USE_NEMA_GFX` |

Every one is `#if`-guarded and every guard is off, so the build is correct today.
**`LV_USE_FS_FROGFS` and `LV_USE_NEMA_GFX` must stay 0** — and not merely as a
config preference. Turning either on is not a config change; it is a
missing-source build failure, because the code it needs was deliberately deleted
for licence reasons. Do not "fix" such a failure by restoring the directory.

> ★ **`LV_USE_DRAW_VG_LITE` is the exception, corrected 2026-08-16.** It used to
> be listed above alongside the other two, on the reasoning that turning it on
> could only end in a missing-source failure. That is true only when
> `LV_USE_VG_LITE_DRIVER` is 1. Both files above select their `vg_lite.h` three
> ways, and the **default branch is `#include <vg_lite.h>` from the include
> path**:
>
> ```c
> #if LV_USE_VG_LITE_THORVG      → others/vg_lite_tvg/vg_lite.h
> #elif LV_USE_VG_LITE_DRIVER    → libs/vg_lite_driver/inc/vg_lite.h   (pruned)
> #else                          → #include <vg_lite.h>                (external)
> #endif
> ```
>
> So `LV_USE_DRAW_VG_LITE=1` with `LV_USE_VG_LITE_DRIVER=0` compiles against an
> externally supplied driver and touches nothing pruned. That is the sanctioned
> hook, and it is how the evkb tree reaches the GC355 — the MIT driver lives in
> its own `VGLite` sibling repo and arrives on the include path. **No LVGL
> source edit, no un-pruning, firewall untouched.**
>
> What must stay 0 is **`LV_USE_VG_LITE_DRIVER`**. Setting that one restores the
> dependency on the dual-licensed copy that was pruned, which is the actual
> licence hazard the original rule was reaching for.

## Retained with a written justification: the one MPL file

`lvgl/src/libs/thorvg/tvgLottieInterpolator.cpp` is MIT-headered ThorVG code
that embeds one MPL-2.0 snippet (the Firefox cubic-bezier solver) inside
`#if LV_USE_THORVG_INTERNAL`, which is 0. Since the audit now flags MPL, this
file is named on the `ALLOW` list in `tools/license-audit.sh` with that
reasoning, rather than passing unnoticed. It is not compiled: `evkb.cmake` globs
`lvgl/src/*.c` only and deliberately never `*.cpp`, which is what all 47 thorvg
files are. Because the allowlisted path is a `.cpp`, the audit's Part 2 holds it
to the empty-object rule — so enabling thorvg would *fail the audit* rather than
quietly link MPL code.

It survives re-vendoring by path, so no action is needed on re-vendor unless
upstream moves or renames the file (in which case the audit fails loudly and the
`ALLOW` entry must be updated, not deleted).

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
   The gate now covers MPL-2.0 and unlicensed prebuilt binaries as well as
   GPL/LGPL, so a re-vendor that restores `frogfs/` or `nema_gfx/` fails rather
   than needing a human to remember. It is still a *tracked-source* check: it
   says nothing about a binary whose adjacent licence text is present but
   unacceptable, so read any new licence file the audit accepts.
   (`tools/license-audit.test.sh` is the negative-test suite proving both checks
   actually fire; run it after touching the audit.)
5. Confirm `LV_USE_VG_LITE_DRIVER`, `LV_USE_FS_FROGFS` and `LV_USE_NEMA_GFX` are
   still 0 in `port/lv_conf.h`. (This step used to name `LV_USE_DRAW_VG_LITE`;
   corrected 2026-08-16 — see the note under "These prunes delete code that
   `src/` still references". `LV_USE_DRAW_VG_LITE` is a scope choice, and it
   will legitimately go to 1 when LVGL's VGLite backend is enabled against the
   external MIT driver. `LV_USE_VG_LITE_DRIVER` is the one that must not move,
   because it re-points the includes at the pruned dual-licensed copy.)
6. Re-record the golden `LVGL_SUM` values in the example gates — a renderer or
   font change legitimately changes them:
   - `examples/display/lvgl_ili9341_test/run_qemu.sh` — currently `0xA087211C`.
     This one is corroborated by a human eye on real glass, and the silicon
     checksum matched the QEMU value bit-for-bit, so re-recording it means
     re-confirming the picture on hardware, not just re-running the gate.
   - `examples/display/lvgl_rpi_panel_test/run_qemu.sh` — currently `0xB220E6E4`.
     That panel is not currently connected, so this golden pins reproducibility
     only; it has never been checked against a real picture.
