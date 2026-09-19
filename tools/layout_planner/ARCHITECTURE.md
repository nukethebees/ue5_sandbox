# Memory layout planner architecture

## Structure

The headless planner library lives in `native/layout/lib/`; its GoogleTest suite is in
`native/layout/tests/`. The optional SDL3 and Dear ImGui frontend lives in
`tools/layout_planner/app/`, with GUI panels in `app/gui/` and SDL-specific headers in
`app/platform/`.

```text
native/layout/lib       headless model, analysis, LispB import, and variants
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

LispB files are loaded through the existing LispB parser and semantic schema infrastructure into a
planner catalog. `LayoutWorkspace` keeps the immutable baseline plus session-only variants.
`Analyzer` combines the selected layout, variant overrides, and an ABI profile to produce factual
packed-value or SoA analysis results. The GUI owns selections, dock layout, cached presentation
results, and drawing; it does not own the planner model or calculate layouts.

The application is read-only with respect to LispB. There is currently no serializer, source
write-back, or persistent variant format.

## Supported analysis and limits

V1 supports packed storage type/bit-range analysis and flat standard-library SoA payload and
64-byte cache-line analysis. The built-in ABI profile provides facts for common fixed-width
integer, floating-point, and Boolean types. Unknown physical types and nested SoAs produce
diagnostics rather than guessed results.

AoS padding, arbitrary ABI probing, persistent plans, LispB write-back, chunking/AoSoA, arena
planning, performance prediction, and live-process inspection are intentionally deferred.

## Frontend behavior

The SDL window provides dockable Project / Schema, Layout, Properties, Variants, and Analysis
panels. The renderer follows recent activity: it runs smoothly during interaction, reduces its
rate while idle or unfocused, and waits for events while minimized.
