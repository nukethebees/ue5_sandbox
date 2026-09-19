# Memory layout planner

The memory layout planner inspects and compares physical memory layouts derived from LispB schemas.
It is a standalone Windows application and does not depend on Unreal Engine, Qt, or C#.

See [architecture](ARCHITECTURE.md) for implementation and dependency details.

## Build and executable location

From the repository root, initialize the optional UI dependencies and run the dedicated workflow:

```powershell
git submodule update --init native/third_party/sdl native/third_party/imgui
cmake --workflow --preset layout-planner
```

The workflow builds and tests the planner. The executable is worktree-local:

```text
%REPOSITORY_ROOT%\out\build\layout-planner\tools\layout_planner\app\layout-planner.exe
```

`%REPOSITORY_ROOT%` is the root of the current worktree. The planner is not installed system-wide
or added to `PATH`.

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

## Getting started

1. Start the planner from the repository root. The Project / Schema panel lists the supported LispB
   packed values and standard-library SoAs.
2. Select `EntityUniqueId` to inspect its proportional packed-bit layout, or `WorldAABBsColumns`
   to inspect its six SoA columns.
3. The baseline is read-only. Use **Create editable variant** directly in Properties to begin an
   in-memory experiment; Variants remains available for naming, duplication, reset, and deletion.
4. In Properties, change a packed field width or storage type, or change an SoA capacity or column
   type. Schema values, planning values, and active overrides are shown separately.
5. Layout shows linked packed-bit, aggregate column-payload, and cache-line views. Comparison
   shows baseline, variant, and factual deltas without ranking either representation.

The planner supports packed values, flat standard-library SoAs, and standard-library vector SoAs.
Generated LispB enums and packed values inherit facts from their declared underlying or storage
types. Unknown types and unsupported schemas are reported as diagnostics rather than guessed.

Column diagrams show aggregate payload only. They do not imply that standard-library vectors share
an allocation or model allocator overhead, capacity slack, or generated single-allocation gaps.

Variants exist only in memory. The application never writes the project manifest or production
LispB files; closing it discards every variant. See [architecture](ARCHITECTURE.md) for supported
constructs, technical limits, and implementation details.
