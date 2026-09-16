# AGENTS.md

Repository guide for coding agents working in this tree. Facts below were read
out of the working tree at **upstream master 2026-09-14 (LVGL 9.5.0)**.

## 1. What this repository is

`lv_port_pc_visual_studio` is the **MSBuild / Visual Studio host** for LVGL on
Windows, not an LVGL fork. LVGL and FreeType are git submodules; this repository
contributes the Win32 host layer, four C++ projects, per-project `lv_conf.h`
profiles, and the MSBuild tooling that keeps the generated file lists and the
DLL export list in step with the submodules.

The CMake-based sibling project is
[lv_port_pc_vscode](https://github.com/lvgl/lv_port_pc_vscode) (SDL2 + GCC/CMake,
native Windows backend still pending in PR #106). Upstream steers CMake users
there and keeps this repository for "traditional Visual Studio (not Code) users".

Runtime dependencies are limited to the Win32 API, the C runtime and the C++ STL.
There is **no test suite** — verification is *build the target, run the exe*.

## 2. Submodules

| Path                    | Remote                                              | Branch tracked        |
| ----------------------- | --------------------------------------------------- | --------------------- |
| `LvglPlatform/lvgl`     | https://github.com/lvgl/lvgl                        | `release/v9.5`        |
| `LvglPlatform/freetype` | https://github.com/freetype/freetype                | (detached pin)        |

```
git submodule update --init --recursive      # to the pinned commits
git submodule update --remote --recursive    # to the latest of the tracked branch
```

Notes:

- `Mile.Project.Windows` **used to be a submodule and was removed** by upstream
  master (2026-09-14). If a checkout still has the directory or a
  `submodule.Mile.Project.Windows` section in `.git/config`, delete both; nothing
  imports it any more.
- `LvglPlatform/` is not purely submodule content: `LvglPlatform/Lvgl.Build.Tasks.dll`
  (committed binary) and `LvglPlatform/LvglWindowsIconResource/*` are tracked by
  this repository. Only `LvglPlatform/lvgl` and `LvglPlatform/freetype` are
  submodules — never patch files inside those two.
- Cloning/building requires network access to github.com (submodules) and
  nuget.org (MSBuild SDK + packages).

## 3. Projects and solutions

Solutions use the modern XML `.slnx` format (upstream's README targets Visual
Studio 2026 and claims VS2022 17.13+ works); the old `LVGL.sln` /
`LVGL.MaintainerTools.sln` were replaced.

| Solution                   | Contents |
| -------------------------- | -------- |
| `LVGL.slnx`                | the four C++ host projects |
| `LVGL.MaintainerTools.slnx`| `LvglProjectFileUpdater`, `Lvgl.Build.Tasks` (C#) |

| Project                             | Kind           | Role |
| ----------------------------------- | -------------- | ---- |
| `LvglWindowsSimulator`              | EXE (startup)  | Simulation host: fixed LVGL resolution (simulator mode), window stretch on DPI change. Compiles LVGL **and** FreeType (`freetype.props`). |
| `LvglWindowsDesktopApplication`     | EXE            | Desktop-app host: resizable window, LVGL resolution/DPI follow the window. Compiles LVGL only. |
| `LvglWindowsStatic`                 | static library | LVGL compiled as a static lib (no host code). |
| `LvglWindows`                       | DLL            | Thin DLL over `$(OutDir)LvglWindowsStatic.lib`; exports come from `LvglWindows\LvglWindows.def`. |
| `Lvgl.Build.Tasks`                  | C# task lib (`netstandard2.0`) | MSBuild tasks: static-library export-list generation, image-archive handling, lv_conf migration. Its `AfterBuild` target copies the built DLL back to `LvglPlatform\`. |
| `LvglProjectFileUpdater`            | C# console (`net10.0`) | Regenerates the `LvglPlatform` item lists inside the C++ projects. |

`LvglWindows.vcxproj` loads the export generator with
`<UsingTask ... AssemblyFile="..\LvglPlatform\Lvgl.Build.Tasks.dll" />`, so the
DLL is a **build input** — the `.def` is produced during the build, not by a
separate tool run.

## 4. Build and run

Prerequisites:

- **Visual Studio 2026** (18.x) is the supported IDE; VS2022 17.13+ also works but
  is not actively supported. Older VS must use
  `upstream/release/v9.4/legacy-vs-solution`.
- The `.vcxproj` files reference the NuGet package **VC-LTL 5.3.1**
  unconditionally: upstream removed the `MileProjectEnableVCLTLSupport` opt-in on
  2026-09-14, so the static-CRT / small-binary toolchain is now always on. YY-Thunks
  is gone.
- They also import the MSBuild project SDK
  `<Import Sdk="Mile.Project.Configurations" Project="…" />`, resolved from
  nuget.org through `global.json`:
  `{ "msbuild-sdks": { "Mile.Project.Configurations": "1.1.2116" } }`.
  `Microsoft.Build.NuGetSdkResolver` ships with VS/MSBuild, so **the .NET SDK is
  not required to compile C++** — but the first build must be able to reach
  nuget.org, and `-restore` (or loading the solution in VS, which restores
  automatically) is needed once. Both land in `%USERPROFILE%\.nuget\packages\`.
- The C# tools target `net10.0` / `netstandard2.0`; running them requires the
  .NET 10 SDK.

Interactive (README path): open `LVGL.slnx` in Visual Studio, set
`LvglWindowsSimulator` as the startup project, press the Local Windows Debugger
button. Visual Studio preselects **ARM64** because it sorts first — check the
platform box.

Command line (verified on this machine):

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" `
  LvglWindowsSimulator\LvglWindowsSimulator.vcxproj -restore `
  -p:Configuration=Debug -p:Platform=x64 -m
```

Again for an incremental build, dropping `-restore`. Always pass
`-p:Configuration` and `-p:Platform`; the projects have no default configuration.

Full matrix (wipes `Output\`, picks the newest VS that has the x86/x64 C++
tools via `vswhere -latest`, then builds `BuildAllTargets.proj` = Debug/Release ×
x86/x64/ARM64 in one pass, `StopOnFirstFailure`):

```
BuildAllTargets.cmd
```

Output layout: `Output\Binaries\<Config>\<Platform>\` for binaries,
`Output\Objects\<Config>\<Project>\<Platform>\` for intermediates.

Trap: `BuildAllTargets.cmd` and CI both compile **ARM64**, which needs the
`Microsoft.VisualStudio.Component.VC.Tools.ARM64` component. Without it those two
legs fail and, because of `StopOnFirstFailure`, can abort the run.

## 5. Code map

- `LvglWindowsSimulator/LvglWindowsSimulator.cpp` — `main()` initializes LVGL,
  creates one display through `lv_windows_create_display(..., simulator_mode = true)`,
  acquires pointer/keypad/encoder input devices and loops on `lv_timer_handler()`
  + `Sleep`. Demo/test entry points are selected by editing the single call in
  `main()`; the active one is `hud_demo()`, which previews the minimal HUD
  (§ `hud.h` / `hud_minimal.c` below) at its real size inside the 800x480
  display. The older attitude-indicator prototype `vhud()` — `lv_roll_scale()`,
  `guide_lines()`, `update_ai()`, the slider callbacks
  `rollSlider_event_cb` / `pitchSlider_event_cb`, plus the integer `sin_milli()`
  / `sin_interp()` helpers and the `sin_table_0_90[]` table they use — is still
  compiled but no longer called from `main()`.
  `update_ai()` rotates `card` through `transform_rotation` and offsets it with
  `transform_translate_x/y` (never `lv_obj_set_pos()`, which would replace the
  centered layout position). The `roll:`/`pitch:`/`offset` labels are only filled
  by `update_ai()`, which runs from slider events — at startup they still show
  LVGL's default `Text`. Note `lv_slider_set_value()` does **not** emit
  `LV_EVENT_VALUE_CHANGED` (only the input paths do), so driving the HUD from
  code/data requires calling the update path explicitly or using subjects.
- `LvglWindowsSimulator/hud.h` + `hud_minimal.c` — the minimal HUD (reference
  design #2) and the code intended to be ported to the ESP32-S3.
  `hud_minimal_create(parent, theme)` builds the 320x172 panel out of `lv_obj` /
  `lv_label` widgets; `hud_data_t` + `hud_minimal_update()` write every value
  (integers only: deci-degrees, whole km/h, cm/s, millivolts). Its two draw
  callbacks must respect two LVGL rules that cost hours to rediscover:
  `lv_draw_*` works in **absolute screen** coordinates (the root's origin is
  added explicitly), and a draw task outlives the callback, so label text must
  point at literals with `text_static = 1` (`text_local` would malloc per draw).
  The pitch ladder's rungs and their value labels are painted from one set of
  coordinates for exactly that reason; the rotating card clips the horizon and
  the opaque top/bottom bars are created last so the overflow never shows.
  The two big readout blocks are a fixed 96 px wide, so `pick_num_font()` gives
  them the largest of `font_xl`/`font_xs`/`font_l` whose rendered width fits —
  a 4 digit altitude must not be allowed to reflow the layout. Bars chain their
  neighbours with `lv_obj_align_to()` rather than hand measured offsets for the
  same reason.
  The horizon does not run under the numbers: two dark panels cover the outer
  part of the middle band and fade into the window, each done as **one** object
  with a horizontal two stop background gradient (`bg_grad_opa` carries the
  alpha per stop), so no extra buffer or blend pass is needed. Every layout
  constant (bar heights, horizon at 60 % of the band, 1.9 px/deg, the 5 degree
  ladder, roll radius 64, the 55/130 px shade ramp) was measured off
  `design/ChatGPT_vhud_design.png`; the table is in
  `Documents/Lvgl95Review-And-ESP32S3Porting.md` §2.7.
- `LvglWindowsSimulator/ui.cpp` + `ui.h` — earlier canvas-based HUD (`ui()`,
  `canvas_fresh()`, `init_sg()`, `init_leftSideBox()`, `init_rightSideBox()`,
  `init_arrow()`, `init_leftMark()`…). Not called from `main()`; kept compiled.
- Cleaned up on 2026-09-16: `ui2.cpp` (dead, uncompiled and unstoppable to link),
  `vhud2()`, `vhud3()`, the half-finished rotation-animation chain
  (`anim_canvas*` / `init_anim*` / `anim_completed_cb`), the unused file-scope
  `style_sky`/`style_ground`, `CANVAS_WIDTH/HEIGHT`, `<math.h>` and the
  `%.2f` formatting in `update_ai()` were all removed.
- Three independent `lv_conf.h` copies (one per host project) — see §6.
- `Documents/Lvgl95Review-And-ESP32S3Porting.md` — fork-local memo: review of
  `vhud()` against LVGL 9.5 best practices, plus the ESP32-S3 porting checklist
  (rotating-layer cost, `lv_conf.h` deltas, mandatory pre-port fixes).

## 6. `lv_conf.h` profiles

Each project compiles its own LVGL copy against its own profile; the files are
not shared and not generated.

- `LvglWindows/lv_conf.h` and `LvglWindowsDesktopApplication/lv_conf.h` are
  currently byte-identical to upstream.
- `LvglWindowsSimulator/lv_conf.h` = upstream file **plus six local overrides**
  (`LV_FONT_MONTSERRAT_12 1`, `LV_FONT_MONTSERRAT_18 1`,
  `LV_FONT_MONTSERRAT_40 1`, `LV_FONT_MONTSERRAT_48 1`,
  `LV_USE_DEMO_RENDER 1`, `LV_USE_DEMO_TRANSFORM 1`; 14/20/24/26 are upstream).
  `LV_FONT_MONTSERRAT_12` is required by `ui.cpp` and by the minimal HUD's small
  captions, `_48` by its big readouts and `_40` by `pick_num_font()`'s fallback
  for long values (`_20`/`_14` are upstream defaults it also uses); `_18` is
  currently unreferenced, and the two demos are only reachable through the
  commented-out `lv_demo_*` calls in `main()`.
- Upstream's authoritative Windows values live in
  `Documents/DefaultLvglConfigurations.md` (color depth 32, C-library stdlib,
  256 KiB heap, 10 ms refresh, `LV_OS_WINDOWS`, log to printf, perceptual/memory
  monitors, `LV_USE_FS_WIN32`, widgets + benchmark demos). Deviating locally is
  allowed but every deviation will re-conflict on the next upstream sync,
  because upstream regenerates this file wholesale.

## 7. Maintainer tools

`Documents/HowToSynchronizeLvglRelatedSubmodules.md` prescribes
`git submodule update --remote` → **`LvglProjectFileUpdater`** → regenerate
`LvglWindows\LvglWindows.def`. That document is **partially stale**: it still
names `LvglModuleDefinitionGenerator`, which upstream deleted in favour of the
`Lvgl.Build.Tasks` MSBuild task that runs during the build.

What the tools actually do:

- `LvglProjectFileUpdater/Program.cs` enumerates `LvglPlatform/lvgl` (and
  `LvglPlatform/freetype` for the simulator) and removes/re-adds **only** the
  items whose `Include` starts with `$(MSBuildThisFileDirectory)..\LvglPlatform\`
  in `LvglWindowsSimulator` and `LvglWindowsDesktopApplication` `.vcxproj` +
  `.filters`; `LvglWindowsLibraryProjectUpdater.cs` does the same for
  `LvglWindowsStatic.vcxproj`. Project-local sources (`LvglWindowsSimulator.cpp`,
  `ui.cpp`, …) are never touched, so **new local files must be added to the
  project by hand** (Visual Studio, or an explicit `ClCompile` entry).
- The `.def` export list is generated at build time by
  `Lvgl.Build.Tasks.StaticLibraryExportsDefinitionGenerator`
  (`LvglWindows.vcxproj`), fed from the static library. Never hand-edit
  `LvglWindows\LvglWindows.def` or the `LvglPlatform` item groups — they are
  generated.
- Both C# tools are console apps launched interactively; the tools project
  targets `net10.0`, so they need the .NET 10 SDK installed.

## 8. Local fork deltas (this checkout vs `lvgl/lv_port_pc_visual_studio@master`)

Recorded so future upstream syncs know what is intentionally different:

- `LvglWindowsSimulator/LvglWindowsSimulator.cpp`, `ui.cpp`, `ui.h` —
  custom HUD experiment code (see §5, including `hud_demo()`). `ui2.cpp` used to
  be part of this list and was deleted on 2026-09-16.
- `LvglWindowsSimulator/hud.h`, `hud_minimal.c` — the minimal HUD (reference
  design #2) and the module earmarked for the ESP32-S3 port, plus its
  `ClCompile`/`ClInclude` entries in the two `LvglWindowsSimulator` project
  files (project-local sources are never regenerated, see §7).
- `design/` — the mock-up sheet and its four per-design crops (1:1 pixels, card
  plus its own title line, no resampling). Read the single file you need instead
  of the 1712x919 sheet: `design1_classic_pfd.png` (方案 1, classic PFD,
  speed/altitude tapes), `design2_minimal_hud.png` (方案 2, the minimal HUD this
  fork implements), `design3_dashboard.png` (方案 3, instrument dashboard),
  `design4_nav_task.png` (方案 4, navigation/mission page). The sheet is a
  stretched mock-up: its horizontal and vertical pixel scales differ, so measure
  each axis separately (the measured constants are tabulated in
  `Documents/Lvgl95Review-And-ESP32S3Porting.md` §2.7).
- `LvglWindowsSimulator/lv_conf.h` — the six overrides listed in §6.
- `LvglWindowsSimulator/LvglWindowsSimulator.cpp` — four
  `lv_obj_remove_flag()` calls cast the OR-ed flags: `(lv_obj_flag_t)(…)`.
  LVGL 9.5 keeps `lv_obj_flag_t` a plain C enum, so a C++ TU can no longer pass
  the promoted `int`.
- `LvglWindowsSimulator/LvglWindowsSimulator.cpp` — `update_ai()` uses the
  integer sine table (`sin_milli()`/`sin_interp()`) instead of `sin()`/`cos()` +
  `%.2f`, and offsets `card` with `transform_translate_x/y` instead of
  `lv_obj_set_pos()`.
- `AGENTS.md`, `.gitignore` (`.codegraph/`), `.vscode/*`.

### Branch model

- `master` — **pristine mirror** of `lvgl/lv_port_pc_visual_studio@master`. Never
  commit here; it is only fast-forwarded from `upstream/master` and pushed.
- `develop` — the long-lived integration branch holding every delta listed above
  (should be the fork's default branch on GitHub).
- `feature/*` — short-lived branches off `develop`, merged back with `--no-ff`.

Sync routine (`master` never conflicts):

```bash
git switch master
git fetch upstream --prune
git merge --ff-only upstream/master        # fails if master was touched locally
git push origin master

git switch develop
git merge master                           # expect a conflict ONLY in lv_conf.h
git submodule update --init --recursive    # branch switches can change submodule pins
```

Resolve the `lv_conf.h` conflict by taking upstream's file and re-applying the
six overrides in §6. Use merge (not rebase) on `develop` — it is a pushed
branch. `git diff upstream/master..develop` is the complete list of local
changes.

## 9. Conventions

- `.editorconfig` is authoritative: UTF-8 **with BOM**, CRLF, 4-space indent for
  C/C++/C#, 2-space for JSON/XML, final newline, Doxygen `/** @brief … */`
  comments (`vc_generate_documentation_comments = doxygen_slash_star`).
- C# files and build scripts start with the Mouri banner
  (`## PROJECT:` / `## FILE:` / `## PURPOSE:` / `## LICENSE:` / `## MAINTAINER:`).
- Match surrounding code instead of introducing a second style: the simulator HUD
  code calls the LVGL C API bare, while the library/application projects qualify
  calls with `::`.
- Keep UI/experiment code inside the simulator project; the library and DLL
  projects stay UI-agnostic.
- Solutions are `.slnx`; MSBuild SDK versions are pinned in `global.json`.

## 10. Code graph

This checkout is indexed with CodeGraph (project-local index in `.codegraph/`,
generated — do not commit):

```
codegraph status              # index freshness and statistics
codegraph files               # file/symbol structure from the index
codegraph query <search>      # symbol lookup
codegraph explore <area>      # symbols + call paths + source for one area
codegraph node <name>         # one symbol's source, callers and callees
codegraph impact <symbol>     # reverse-dependency analysis before refactors
codegraph sync                # incremental refresh after edits
codegraph install             # register the MCP server with another agent
```

The index covers this repository's sources; submodule sources are indexed after
`git submodule update --init --recursive` plus `codegraph index`.
