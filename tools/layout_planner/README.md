# Memory layout planner

The memory layout planner inspects and compares physical memory layouts derived from LispB schemas.
It is a standalone Windows application and does not depend on Unreal Engine, Qt, or C#.

See [architecture](ARCHITECTURE.md) for implementation and dependency details.
See the [roadmap](ROADMAP.md) for the path from inspection and experiments to full LispB type
authoring.

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

Window size, panel placement, dock split ratios, and text size are saved in the per-user application
preferences directory and restored on the next launch. Maximized, minimized, and fullscreen sizes
do not replace the last normal window size. Use **View > Reset panel layout** to return to the
default panel arrangement.

## Getting started

1. Start the planner from the repository root. The Project / Schema panel lists LispB enums, packed
   values, records, and supported standard-library SoAs from the shared semantic graph.
2. Select `EntityUniqueId` to inspect its proportional packed-bit layout, or `WorldAABBsColumns`
   to inspect its six SoA columns.
3. Enum, packed-value, record, and standard-library SoA baselines are editable LispB declarations.
   Their inline Properties editors issue validated semantic commands and participate in File >
   Undo/Redo. Record members support scalar or fixed-array cardinality and update the byte map live.
4. Use **Create editable variant** for an in-memory physical experiment. In a variant, change a
   packed field width/storage type or a SoA capacity/column type without modifying LispB. Schema
   values, planning values, and active overrides are shown separately.
5. Layout shows linked packed-bit, record byte maps, aggregate column-payload, and cache-line views.
   Record maps derive member offsets, fixed-array extents, alignment, internal padding, and tail
   padding from the active target profile. For packed values,
   choose an element-count preset or custom count to see overflow-safe storage, unused-bit,
   cache-line, and page totals from the explicit target profile. Comparison lets you select any two
   variants as A and B and shows factual deltas without ranking either representation. B follows the
   actively edited variant until you choose or swap it explicitly.
6. Graph shows the resolved semantic types and their labeled underlying/storage/field/column
   dependencies. Middle-drag to pan, use the wheel to zoom, and click a node to navigate to it.

The planner supports enum inspection, packed values, records, flat standard-library SoAs, and
standard-library vector SoAs. Enum-backed packed fields retain links to their enum definitions;
physical facts are derived separately from declared underlying or storage types. Unknown types and
unsupported schemas are reported as diagnostics rather than guessed.

Enum Properties derives the known numeric domain from literal and implicit values. It separates
live symbols from the count sentinel, reports the minimum semantic bit width and unused backing
codes, and validates the domain against explicit signed/unsigned target facts. Initializer
expressions that cannot be evaluated safely remain Unknown with an explanatory diagnostic.

Column diagrams show aggregate payload only. They do not imply that standard-library vectors share
an allocation or model allocator overhead, capacity slack, or generated single-allocation gaps.
Per-column and aggregate minimum page counts use the target profile's page size; the aggregate sums
the separate standard-library vector allocations rather than pretending that they share pages.

Layout variants exist only in memory and closing the application discards them. Semantic authoring
changes remain in an editable draft until the user explicitly previews and saves them from the File
menu. See [architecture](ARCHITECTURE.md) for supported constructs, technical limits, and
implementation details.

The File menu can open a LispB project manifest by path, switch among the 20 most recently opened
projects, save the current draft, or save it as an independent clone. The most recent successful
project is reopened on the next normal launch; an explicit `--project` command-line argument takes
precedence. Until a native file picker is added, Open and Save As use path text boxes. Save As writes
a new project manifest plus a sibling `<name>_schema` directory containing the current schema
target's types and module sources, validates the clone, and then switches the planner to it. Existing
destinations are not overwritten.

The Project / Schema browser is organized by LispB module. Each module contains its declared enums,
packed values, or supported SoA types, matching source ownership and the destination used when new
types are authored.

## Semantic authoring

Enum authoring is available as an inline table:

1. Select **+ New enum** in Project / Schema.
2. Choose an existing enum module, enter the declared name and underlying type, and create it.
3. In Properties, add, duplicate, delete, reorder, or drag values. Select a row to edit its symbolic
   name, explicit value, display name, serialized name, hidden marker, or count-sentinel role inline.
4. Use **File > Undo** and **File > Redo** while designing.
5. Use **File > Preview LispB changes** to compare the original and complete updated source.
6. Use **File > Save LispB changes** to validate the rendered sources, replace the affected source
   file, and reload the semantic document.

Packed-value authoring follows the same path. Use **+ New packed value**, choose an existing packed
module and storage type, then edit storage/invalid value and add, duplicate, delete, reorder, or drag
fields inline. Field name, semantic type, bit width, kind, and range-helper metadata are editable.
The baseline bit map permits divider dragging; releasing a divider commits both adjacent widths as
one undoable semantic command.

Standard-library SoAs can likewise be created with **+ New SoA**. Columns support inline name,
semantic type, and array/nested kind editing plus add, duplicate, delete, button reorder, and drag
reorder. Capacity remains a planner experiment setting and is not written into LispB.

Creation currently targets an existing module of the matching kind; creating modules, renaming
types, and deleting existing source declarations are not yet exposed. Saving an edited existing enum,
packed value, or SoA renders that declaration canonically, so review the preview for comments or
hand formatting inside the edited declaration. Unrelated declarations and files are left unchanged.

Closing the application with dirty semantic edits offers Save, Discard, and Cancel choices. The
Graph view can be shown or hidden from View; docking and visibility are persisted.
