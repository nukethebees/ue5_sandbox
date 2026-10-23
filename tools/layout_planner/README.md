# Memory layout planner

A standalone Windows editor and memory-layout analyzer for LispB schemas. It uses
SDL and ImGui and does not require Unreal Engine.

## Build and run

From the repository root, use the `coj` worktree and jobs-board workflow described
in [AGENTS.md](../../AGENTS.md), then build and test:

```powershell
cmake --workflow --preset layout-planner
```

The optional SDL, ImGui, and native-file-dialog submodules must be initialized.
Run the worktree executable with a project manifest and schema target:

```powershell
.\out\build\win-x64-clangcl-debug\tools\layout_planner\app\layout-planner.exe --project lispb/project.lispb --target sandbox-code
```

Without `--project`, the application reopens its recent project, or initially
loads `lispb/project.lispb` with the `sandbox-code` target. Use `--help` for options.
For focused native validation after a build:

```powershell
ctest --test-dir out/build/native -L layout -LE "soak|compile-contract" --output-on-failure
```

The dedicated planner workflow also runs the UI tests. The CMake install
component `layout-planner` installs the executable under the selected prefix's
`bin` directory.

Build `native-layout-benchmarks` in Release to measure edits, undo and redo with
bulk analysis at fixed history depths. Run the Google Benchmark executable from
`native/layout/benchmarks/` in that build directory; standard benchmark filtering,
repetition and JSON output flags are supported. Request an exclusive jobs-board
ticket before timing runs.

## Edit a document

1. Enter a manifest path and target in **Project**, then choose **Open project**.
   **New project** starts with an empty `game` module; enter a destination and
   target and use **File > Save as** to publish it.
2. Select a declaration in the module browser. Edit its canonical LispB in
   **Declaration**, then choose **Apply**. The entire declaration is validated
   as one transaction. A rejected edit leaves the document unchanged and reports
   errors in **Diagnostics**.
3. Use **Quick edit** for packed widths/storage and member types. Named
   rename, duplicate, and move operations handle references through the document
   API. **New declaration** accepts any supported LispB declaration form.
4. Use the **Sources**, **Create module**, and **Selected module** sections for
   source-file and module changes. Selecting a source opens its complete LispB;
   **Apply source** edits settings, umbrella headers, module properties, or
   several declarations together. These participate in the same undo history.
5. **Edit > Undo/Redo** or Ctrl+Z/Ctrl+Y navigate completed transactions.
   **File > Save** or Ctrl+S writes the document. Apply text drafts before saving.

Saving canonically formats affected files and retains attached comments and
literal contents. It detects external file changes. Saving does not clear undo
history; undo changes memory until the next save. Generate C++ from saved schemas
using the normal LispB compiler workflow.

Window size, docking layout, and the recent project are retained in application
preferences. Closing with unsaved document changes or an edited text draft asks
whether to discard them.

## Analyze and compare

**Layout** displays the selected declaration's sizes, padding, packed fields,
SoA columns, and encoding metrics. Set element count and requested SoA capacity
to model the intended use. **Access workload** selects a member, operation, and
access multiplicity and reports payload and cache/page footprint estimates.

Load explicit primary and comparison ABI profiles under **Target profiles**.
**Target comparison** shows consequences of those different target facts.
**Dependencies** lists referenced types and their users.

**Variants** contains session-local physical experiments: packed storage/width,
SoA column-type, and capacity overrides. Select an active variant to analyze it;
expand **Compare variants** to compare two variants. Variant changes share
document undo/redo but do not modify saved LispB files.

Element counts, capacities, profiles and access workloads also share
undo/redo. These analysis settings remain session-local. Controls queue changes;
the next frame displays the results of processing the complete batch.

Missing physical facts remain **Unknown**. Semantic bit widths do not imply a
standalone C++ size or alignment. Cache/page figures describe modeled storage
and access footprints, not measured throughput or latency; supplied ABI and
memory facts determine their validity. Overflow and invalid physical variants
are reported through diagnostics rather than silently clamped.

See [ARCHITECTURE.md](ARCHITECTURE.md) for the document and analysis design.
