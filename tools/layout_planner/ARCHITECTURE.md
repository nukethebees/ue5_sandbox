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
snapshot of the resolved graph plus session-only physical variants; graph replacement remaps
variant overrides through stable type identities. `Analyzer` combines
a selected semantic type, variant overrides, and an ABI profile to produce factual packed-value or
SoA analysis results. It follows enum-underlying and packed-storage references only while deriving
physical facts, preserving the logical nodes and their dependency edges. The GUI owns selections,
dock layout, cached presentation results, and drawing; it does not own schema semantics or calculate
layouts.

Enum and packed-value authoring are write-enabled vertical slices. They can add declarations to
existing matching modules, edit ordered child rows through semantic commands, preview affected
sources, and explicitly save and reload LispB. Packed fields preserve semantic type links while the
layout view derives physical facts and supports undoable schema divider drags. SoA declarations
remain read-only, and there is no persistent variant format.

The application treats a C++ schema target as the open authoring document. Open/recent operations
load its project manifest and target, while Save As materializes the current draft as a validated,
self-contained one-target project clone. The browser follows the semantic manifest's module order
and declaration ownership rather than inventing type-kind folders.

## Supported analysis and limits

The planner supports packed storage type/bit-range analysis plus flat standard-library SoA and
vector-SoA payload analysis. It reports 64-byte cache-line tiling and factual baseline/variant
deltas. The built-in ABI profile provides facts for common fixed-width integer, floating-point,
and Boolean types. Unknown physical types and nested SoAs produce diagnostics rather than guessed
results. The fixed cache-line fact remains planned target-profile work rather than semantic data.

AoS padding, arbitrary ABI probing, persistent plans, LispB write-back, chunking/AoSoA, arena
planning, performance prediction, and live-process inspection are intentionally deferred. A later
composite-layout milestone can compose named typed arrays and existing layout definitions for
structures such as collision-grid or health storage; V2 deliberately adds neither a composite DSL
nor planner persistence.

## Frontend behavior

The SDL window provides dockable Project / Schema, Layout, Properties, Variants, and Comparison
panels. The renderer follows recent activity: it runs smoothly during interaction, reduces its
rate while idle or unfocused, and waits for events while minimized.
