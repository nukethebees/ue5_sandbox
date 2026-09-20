# Memory layout planner architecture

## Structure

The headless planner library lives in `native/layout/lib/`; its GoogleTest suite is in
`native/layout/tests/`. The optional SDL3 and Dear ImGui frontend lives in
`tools/layout_planner/app/`, with GUI panels in `app/gui/` and SDL-specific headers in
`app/platform/`.

```text
native/lispb            parser, validated schema, and shared semantic type graph
native/layout/lib       headless layout analysis, schema loading, and variants
native/layout/tests     GoogleTest coverage of the library
tools/layout_planner/app SDL3/ImGui bootstrap, event loop, and presentation
```

## Dependencies

`native-layout-tests` depends on `native-layout` and GoogleTest. `layout-planner` depends on
`native-layout`, SDL3, and Dear ImGui. The library has no SDL3, Dear ImGui, graphics, windowing,
or application-header dependency.

The normal native configuration builds and tests `native-layout` while the optional planner GUI is
disabled, which prevents UI dependencies from leaking into the library.

## State and data flow

LispB S-expressions remain the canonical source. The existing parser loads and validates a
`codegen::Manifest`, then `lispb-schema` resolves it into the shared semantic `TypeGraph`. C++
codegen and the planner are peer consumers of that graph; the planner does not maintain a second
schema catalog. See the LispB [semantic graph boundary](../../native/lispb/SEMANTIC_TYPE_GRAPH.md).

`EditableSchemaDocument` owns the source-aware mutable declaration draft and resolves a replacement
graph after every accepted semantic command. It provides stable declaration IDs, validation
rollback, undo/redo, source preview, and validated source replacement. `LayoutWorkspace` owns a
snapshot of the resolved graph, the selected aggregate analysis scale, and session-only physical
variants; graph replacement remaps variant overrides through stable type identities. `Analyzer` combines
a selected semantic type, variant overrides, and an ABI profile to produce factual packed-value or
SoA analysis results. It follows enum-underlying and packed-storage references only while deriving
physical facts, preserving the logical nodes and their dependency edges. The GUI owns selections,
dock layout, cached presentation results, and drawing; it does not own schema semantics or calculate
layouts.

Enum, packed-value, and standard-library SoA authoring are write-enabled vertical slices. They can
add declarations to existing matching modules, edit ordered child rows through semantic commands,
preview affected sources, and explicitly save and reload LispB. Packed fields and SoA columns
preserve semantic type links while the layout view derives physical facts. Packed dividers and
enum/packed/SoA drag reorder issue ordinary undoable schema commands. Planner capacity remains
session state rather than becoming a SoA source property, and there is no persistent variant format.

The application treats a C++ schema target as the open authoring document. Open/recent operations
load its project manifest and target, while Save As materializes the current draft as a validated,
self-contained one-target project clone. The browser follows the semantic manifest's module order
and declaration ownership rather than inventing type-kind folders.

## Supported analysis and limits

Ordinary AoS semantics use `record-module` / `record` declarations rather than overloading SoA
columns. A record member references the shared semantic type graph and may have a fixed element
count; C++ lowering is one consumer and emits an ordinary struct plus `std::array` where needed.
Physical member offsets and padding remain target analysis, not durable semantic properties.
The record analyzer recursively derives nested-record facts, fixed-array extents, offsets, object
alignment, and internal/tail padding from the selected ABI profile. Unknown target facts stay
Unknown, while illegal by-value record cycles are rejected during semantic resolution.
Record create/replace/delete commands share the same stable declaration identity, validation
rollback, undo/redo, preview, and atomic save/reload path as enum, packed, and SoA declarations.
Record aggregate analysis uses the planner's selected element count and target profile to report
total storage, member extents, ABI padding, minimum cache lines, and minimum pages. Arithmetic
overflow and absent target facts remain explicit diagnostics/Unknown values.

The planner supports enum value-domain analysis, packed storage type/bit-range analysis, and flat
standard-library SoA/vector-SoA payload analysis. Enum analysis derives implicit literal values,
count-sentinel code use, minimum semantic width, backing fit, and unused backing codes; arbitrary
initializer expressions remain explicitly unknown. Packed analysis reports overflow-safe aggregate
storage, payload bits, unused packed bits, cache lines, and pages at a selected element count. The
built-in x86/x86-64 baseline ABI profile explicitly provides 64-byte cache-line and 4 KiB page facts
with provenance, along with common fixed-width integer signedness, floating-point, and Boolean type
facts. SoA cache tiling now consumes the same profile fact rather than an analyzer constant. Missing
profile facts, unknown physical types, and nested SoAs produce diagnostics and Unknown results
rather than guesses.

For standard-library SoAs, page footprints are calculated per column because each vector is a
separate allocation. The displayed aggregate is the sum of those minimum per-column page counts;
it does not assume shared pages or include allocator overhead.

Strong aliases, arbitrary ABI probing, persistent plans, chunking/AoSoA, arena planning,
performance prediction, and live-process inspection remain incomplete. Planner semantic changes
write back through LispB rather than a parallel persistence format.

## Frontend behavior

The SDL window provides dockable Project / Schema, Layout, Graph, Properties, Variants, and
Comparison panels. Graph is a navigation-only pan/zoom view over the resolved `TypeGraph`; its
deterministic columns and field-aware edge labels do not duplicate semantic state or assume the
graph is acyclic. The renderer follows recent activity: it runs smoothly during interaction,
reduces its rate while idle or unfocused, and waits for events while minimized.
