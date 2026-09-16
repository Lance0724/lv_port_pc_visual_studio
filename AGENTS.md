# AGENTS.md

Repository guide for coding agents working in this tree. Every claim below was
read out of the working tree; the submodule-resident facts are limited to what
the checked-in project files and `lv_conf.h` profiles assert, because the
submodules themselves are currently checked out empty (see §2).

## 1. What this repository is

`lv_port_pc_visual_studio` is the **Windows host** for LVGL v9, not an LVGL fork.
LVGL, FreeType and the shared MSBuild property sheets live in git submodules;
this repository contributes only:

- three Win32 host projects (simulator EXE, desktop-application EXE, DLL +
  static library),
- three independent `lv_conf.h` build profiles,
- the C# maintainer tools that regenerate the Visual Studio file lists and the
  DLL export list from the submodules.

Runtime dependencies are intentionally limited to the Win32 API, the C runtime
and the C++ STL (`README.md`). There is **no test suite and no linter gate** —
"verification" here means *build the target and run the executable*.

## 2. Submodules are a hard prerequisite

| Path                     | Remote                                                |
| ------------------------ | ----------------------------------------------------- |
| `LvglPlatform/lvgl`      | https://github.com/lvgl/lvgl                          |
| `LvglPlatform/freetype`  | https://github.com/freetype/freetype                  |
| `Mile.Project.Windows`   | https://github.com/ProjectMile/Mile.Project.Windows   |

```
git submodule update --init --recursive
```

If a submodule directory is empty, **nothing builds** — this is not a degraded
mode:

- `Directory.Build.props` imports
  `Mile.Project.Windows\Mile.Project.Build.props`, so MSBuild fails while
  evaluating properties.
- Each `*.vcxproj` imports `Mile.Project.Platform.{x86,x64,ARM64}.props`,
  `Mile.Project.Cpp.Default.props` and `Mile.Project.Cpp.props` from the same
  submodule.
- `LvglPlatform/lvgl` and `LvglPlatform/freetype` provide *every* source file
  compiled by the host projects; the checked-in item lists point into
  `$(MSBuildThisFileDirectory)..\LvglPlatform\`.

Never edit files under `LvglPlatform/`: they are submodule content. LVGL
upgrades flow through `git submodule update --remote` plus the maintainer tools
in §6.

## 3. Projects

| Project                             | Kind                | Role |
| ----------------------------------- | ------------------- | ---- |
| `LvglWindowsSimulator`              | EXE (startup)       | Simulation host. Fixed LVGL resolution (`simulator_mode = true`), window stretching on DPI change, FreeType compiled in. Owns LVGL *and* FreeType sources. |
| `LvglWindowsDesktopApplication`     | EXE                 | Desktop-app host. Resizable window, LVGL display resolution and DPI follow the window. Compiles LVGL only (no `freetype.props`). |
| `LvglWindowsStatic`                 | static library      | LVGL compiled as a static lib, no host code. |
| `LvglWindows`                       | DLL                 | Thin DLL over `$(OutDir)LvglWindowsStatic.lib`, exports the list in `LvglWindows\LvglWindows.def`. |
| `LvglProjectFileUpdater`            | C# console          | Regenerates the `LvglPlatform` item lists in the `*.vcxproj`/`*.filters`. |
| `LvglModuleDefinitionGenerator`     | C# console          | Regenerates `LvglWindows\LvglWindows.def` from the built static lib. |

`LVGL.sln` builds the four C++ projects (Debug/Release × x86/x64/ARM64);
`LVGL.MaintainerTools.sln` builds the two C# tools.

## 4. Build and run

Interactive (the documented path, `README.md`):

1. Open `LVGL.sln` in Visual Studio 2022.
2. Set `LvglWindowsSimulator` as startup project and start the Local Windows
   Debugger.

Full matrix from a shell (`Output/` is wiped first):

```
BuildAllTargets.cmd
```

which calls `Mile.Project.Windows\InitializeVisualStudioEnvironment.cmd` and then
`MSBuild -binaryLogger:Output\BuildAllTargets.binlog -m BuildAllTargets.proj`
(`Restore` + `Build` over all six configuration/platform combinations,
`PreferredToolArchitecture=x64`).

Single project, from a `vcvars64` shell (this is what `.vscode/tasks.json`
"MSBuild" does):

```
MSBuild.exe LvglWindowsSimulator\LvglWindowsSimulator.vcxproj -m
```

CI (`.github/workflows/CI.yml`) runs `msbuild /m BuildAllTargets.proj` on
`windows-latest` with recursive submodules and uploads `Output` as an artifact.

Two README-known traps:

- Visual Studio preselects **ARM64** because it sorts first; check the platform
  box before building.
- `LV_MEM_SIZE` must be ≥ 128 KiB; the simulator and application profiles use
  256 KiB, the library profile only 64 KiB.

## 5. Code map and the three `lv_conf.h` profiles

### Simulator sources

- `LvglWindowsSimulator/LvglWindowsSimulator.cpp` — `main()` starts the LVGL
  loop (`lv_timer_handler` + `Sleep`), creates one `lv_display_t` via
  `lv_windows_create_display(..., simulator_mode = true)` and registers pointer,
  keypad and encoder input devices. Test entry points are selected by editing
  the single call in `main()`; the current one is `vhud()`. Live HUD functions:
  `vhud()`, `lv_roll_scale()`, `guide_lines()`, `update_ai()`, the slider
  callbacks `rollSlider_event_cb` / `pitchSlider_event_cb`, and the rotation
  animation `anim_canvas_cb` / `init_anim_obj` / `init_anim`, which writes a
  transform rotation onto the global `card`.
- `LvglWindowsSimulator/ui.cpp` + `ui.h` — earlier canvas-based HUD
  (`ui()`, `canvas_fresh()`, `init_sg()`, `init_leftSideBox()`,
  `init_rightSideBox()`, `init_arrow()`, `init_leftLineMark()`,
  `init_rightLineMark()`). Currently **not called** (`// ui();` in `main()`).
  It mixes v9 draw-buffer APIs (`LV_DRAW_BUF_DEFINE`,
  `lv_canvas_set_draw_buf`) with older calls (`lv_scr_act()`,
  `lv_canvas_set_buffer()`, `lv_label_set_recolor()`); confirm each API against
  the pinned submodule headers before copying patterns out of this file.
- `LvglWindowsSimulator/ui2.cpp` — a newer grid-based rewrite of the same HUD
  idea (`vhud()` plus `init_anim()` animating a 100×198 sky/ground card).
  **It is not referenced by `LvglWindowsSimulator.vcxproj`, so it is never
  compiled.** Adding it as-is also breaks the link: `vhud()` and `init_anim()`
  are already defined in `LvglWindowsSimulator.cpp`, and the non-`static`
  globals `card`, `style_sky`, `style_ground` exist in both translation units
  (duplicate-symbol / aliasing hazard).

### `lv_conf.h` — three independent, 954-line copies

Each project compiles its own LVGL copy against its own profile; they are **not**
shared and not generated, so a profile change has to be repeated per project.

| Define                    | `LvglWindows` (lib/DLL) | `LvglWindowsSimulator` | `LvglWindowsDesktopApplication` |
| ------------------------- | ----------------------- | ---------------------- | ------------------------------- |
| `LV_MEM_SIZE`             | 64 KiB                  | 256 KiB                | 256 KiB                         |
| `LV_DEF_REFR_PERIOD`      | 33 ms                   | 10 ms                  | 10 ms                           |
| `LV_LOG_PRINTF`           | 0                       | 1                      | 1                               |
| `LV_FONT_MONTSERRAT_12`   | 0                       | 1                      | 0                               |
| `LV_FONT_MONTSERRAT_18`   | 0                       | 1                      | 0                               |
| `LV_FONT_MONTSERRAT_24`   | 0                       | 1                      | 1                               |
| `LV_USE_PERF_MONITOR`     | 0                       | 1                      | 1                               |
| `LV_USE_MEM_MONITOR`      | 0                       | 1                      | 1                               |
| `LV_BUILD_EXAMPLES`       | 0                       | 1                      | 1                               |
| `LV_USE_DEMO_WIDGETS`     | 0                       | 1                      | 1                               |
| `LV_USE_DEMO_BENCHMARK`   | 0                       | 1                      | 1                               |
| `LV_USE_DEMO_RENDER`      | 0                       | 1                      | 0                               |
| `LV_USE_DEMO_TRANSFORM`   | 0                       | 1                      | 0                               |
| `LV_USE_FREETYPE`         | 0                       | 0                      | 0                               |

Shared across all three: `LV_COLOR_DEPTH 32`, `LV_USE_OS LV_OS_WINDOWS`,
`LV_USE_DRAW_SW 1`, `LV_USE_WINDOWS 1`, `LV_TXT_ENC_UTF8`,
`LV_FONT_DEFAULT lv_font_montserrat_14`.

Note `LV_USE_FREETYPE 0` everywhere even though `freetype.props` compiles the
FreeType library into the simulator and adds
`..\LvglPlatform\freetype\include\` to its include path: the library is present,
LVGL's FreeType text/font path is off until the define is flipped.

## 6. Maintainer tools (submodule synchronization)

`Documents/HowToSynchronizeLvglRelatedSubmodules.md` prescribes:
`git submodule update --remote`, then run **`LvglProjectFileUpdater`**, then
**`LvglModuleDefinitionGenerator`** (both from `LVGL.MaintainerTools.sln`).

What they actually do (read from the sources):

- `LvglProjectFileUpdater/Program.cs` enumerates `LvglPlatform/lvgl` (and
  `LvglPlatform/freetype` for the simulator) and **removes and re-adds only the
  items whose `Include` starts with
  `$(MSBuildThisFileDirectory)..\LvglPlatform\`** in
  `LvglWindowsSimulator` and `LvglWindowsDesktopApplication` `*.vcxproj` and
  `*.filters`; `LvglWindowsLibraryProjectUpdater.cs` does the same for
  `LvglWindowsStatic.vcxproj` and its filters. `LvglWindows.vcxproj` is handled
  by none of the three entry points and is maintained by hand. Project-local
  sources (`LvglWindowsSimulator.cpp`, `ui.cpp`, …) are left untouched, so
  **new local files must be added to the project by hand** (Visual Studio, or an
  explicit `ClCompile` entry).
- `LvglModuleDefinitionGenerator/Program.cs` reads
  `Output\Binaries\Release\x64\LvglWindowsStatic.lib`, keeps symbols starting
  with `lv_` or `_lv_` and rewrites `LvglWindows\LvglWindows.def`. A
  Release|x64 static library must exist first.
- Both are console apps that pause at the end (`Console.ReadKey()` in the
  updater), i.e. they are meant to be launched interactively.

Therefore: **never hand-edit the `LvglPlatform` item groups or
`LvglWindows\LvglWindows.def`** — they are generated. Hand-editing them is
silently reverted on the next run.

## 7. Conventions

- `.editorconfig` is authoritative: UTF-8 **with BOM**, CRLF, 4-space indent for
  C/C++/C#, 2-space for JSON/XML, final newline, Doxygen `/** @brief … */`
  comments (`vc_generate_documentation_comments = doxygen_slash_star`).
- C# files and project/build scripts start with the Mouri banner
  (`## PROJECT:` / `## FILE:` / `## PURPOSE:` / `## LICENSE:` / `## MAINTAINER:`);
  match it in new maintainer-tool files.
- Match the surrounding code rather than introducing a second style: the
  simulator HUD code uses bare LVGL calls, while the library/application
  projects qualify LVGL calls with `::`.
- Keep UI/experiment code inside the simulator project; the library and DLL
  projects stay UI-agnostic.
- README wording, changelog and docs for user-visible behavior changes.

## 8. Code graph

This checkout is indexed with CodeGraph (`codegraph init`, project-local index
in `.codegraph/`, generated — not to be committed):

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

The index currently covers the 15 tracked source/config files of this
repository; submodule sources are indexed only once the submodules are checked
out (`git submodule update --init --recursive`, then `codegraph index`).
