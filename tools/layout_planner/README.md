# Memory layout planner

The memory layout planner is a standalone Windows C++ application for inspecting and comparing
physical memory layouts derived from the project's LispB schemas. It does not depend on Unreal
Engine, Qt, or C#.

## Architecture

The headless planner library lives in `native/layout/lib/`; its GoogleTest suite is in
`native/layout/tests/`. The optional SDL3 and Dear ImGui frontend lives in
`tools/layout_planner/app/`, with GUI panels in `app/gui/` and SDL-specific headers in
`app/platform/`.

`native-layout-tests` depends only on `native-layout` and GoogleTest. `layout-planner` depends on
`native-layout`, SDL3, and Dear ImGui. The planner library has no SDL3, Dear ImGui, graphics, or
windowing dependency.

## Build and executable location

From the repository root, initialize the optional UI dependencies and run the dedicated workflow:

```powershell
git submodule update --init native/third_party/sdl native/third_party/imgui
cmake --workflow --preset layout-planner
```

The workflow builds and tests the planner. It places the executable at:

```text
%REPOSITORY_ROOT%\out\build\layout-planner\tools\layout_planner\app\layout-planner.exe
```

Here, `%REPOSITORY_ROOT%` means the root of the current worktree. For example, a checkout below a
Windows user profile would have a path beginning with `%USERPROFILE%\...`; no particular username
or checkout location is required.

V1 is not installed system-wide, copied to a per-user application directory, or added to `PATH`.
Each worktree owns its build output, so rebuilding another worktree does not replace this
executable.

## Run

Run the planner with the repository root as its working directory:

```powershell
.\out\build\layout-planner\tools\layout_planner\app\layout-planner.exe
```

The default invocation loads `lispb/project.lispb` and its `sandbox-code` target. Override either
selection when inspecting another manifest or target:

```powershell
.\out\build\layout-planner\tools\layout_planner\app\layout-planner.exe `
  --project path\to\project.lispb `
  --target target-name
```

Use `--help` to list command-line options. Project-load and schema diagnostics are reported in the
application and on standard error.

## Supported analysis

V1 imports these concepts through the existing LispB parser and semantic schema model:

- packed values, including storage types, field bit ranges, unsigned limits, unused bits, enum bit
  widths, and invalid raw values;
- flat SoA modules using the standard-library backend, including column payloads, row payload,
  capacity, 64-byte cache-line counts, and elements per cache line;
- common fixed-width integer, floating-point, and Boolean physical facts from the built-in native
  ABI profile.

`EntityUniqueId` and `WorldAABBs` are the representative project schemas used by the integration
tests.

Unknown physical types are reported as diagnostics. The planner does not guess their size or
alignment. Nested SoAs and other unsupported schema constructs are diagnosed instead of being
included in potentially incorrect totals.

## Working with variants

The baseline always reflects the loaded LispB schema. Session variants can override:

- packed-value storage types;
- packed-field bit widths;
- SoA capacity;
- supported SoA member planning types.

Variants can be created, duplicated, renamed, reset, selected, and deleted. Analysis is recomputed
only when planner state changes, and the Analysis panel compares the active variant with its
baseline.

Variants exist only in memory. The application never writes the project manifest or production
LispB source files, and closing the application discards all variants.

## Interface and frame pacing

The single SDL window contains dockable Project/Schema, Layout, Properties, Variants, and Analysis
panels. Docking is enabled; operating-system multi-viewport windows are not.

Rendering follows recent activity rather than continuously running at display cadence:

- interaction and a short post-input tail render smoothly;
- a focused but inactive window renders at a reduced idle rate;
- an unfocused window renders at a low background rate;
- a minimized window waits for SDL events.

Mouse, keyboard, text input, resizing, focus, restore, and quit events wake the application.

## Deliberate V1 limits

The planner does not currently provide LispB write-back, persistent plan files, arbitrary C++ ABI
probing, nested-SoA analysis, chunking/AoSoA design, arena planning, performance prediction, or
live-process inspection. These are future extensions of the native layout model rather than UI
state.
