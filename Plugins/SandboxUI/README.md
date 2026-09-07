# SandboxUI

SandboxUI provides reusable, game-independent runtime UI and rendering primitives. Production code
lives in the `SandboxUI` runtime module. `SandboxUIExamples` and `SbxUIExperiments` are editor-only
consumers, and production code must not depend on them.

## 3D radar

The production radar is implemented by `SRadarWidget`, `FRadarFrameStore`, `FRadarStyle`, and the
RDG renderer under `Source/SandboxUI`. It renders a transparent hex plane, nonlinear-range
honeycomb, structure, altitude stems, and analytical screen-space glyphs in four bounded raster
passes. No actors, sprite components, textures, or scene capture are required.

The data flow is:

```text
Simulation/presentation source -> FRadarFrame triple buffer -> SRadarWidget
                                                              |
                                                              v
                              RDG StructuredBuffer<FRadarInstance>
                                                              |
                                                              v
                              1024x1024 render target -> Slate SImage
```

To embed the radar, create a shared `FRadarFrameStore`, assign it to `SRadarWidget`, populate
`frame_store->next()`, and call `publish()` when the frame is complete. Call `render()` from the
owning UI's update path. Set presentation through `FRadarStyle`; the frame source remains
responsible for contact positions, glyph types, colours, flags, and semantic display radii.

`SpaceGame` integrates the widget through `UShipHudWidget`. Its Widget Blueprint must contain:

- `radar_host`: a `NativeWidgetHost` that receives the runtime `SRadarWidget`.
- `radar_background`: a `Border` occupying the same Canvas Panel area. It is made transparent at
  runtime.

Both widgets must be direct children of a `CanvasPanel`. Their authored Canvas Panel layout is
currently replaced at runtime with a bottom-right anchor, `640 x 640` design-unit size, and `32`
design-unit viewport margin. Unreal then applies the application DPI scale; at `0.67`, for example,
the displayed radar is approximately `429 x 429` screen pixels. Resizing `radar_host` only in the
Widget Blueprint designer therefore does not change its in-game size. These layout values currently
have no editor controls and are defined by `configure_radar_canvas_slot` in `ShipHudWidget.cpp`.

### Level authoring

Gameplay radar settings are edited on the level's `ATestBatchOrchestrator` under
**Sandbox > Presentation > Radar**:

- `Enabled` controls whether the radar is populated and shown.
- `Combat Range`, `Tactical Range`, `Maximum Range`, and `Combat Display Radius` configure the fixed
  nonlinear distance mapping.
- `Grid Opacity` and the four cell-radius settings configure honeycomb visibility and density.
- `Glyph Size Scale` scales all contact cores without changing the radar footprint.
- `Objective Size Multiplier`, `Objective Ring Padding Pixels`, and
  `Objective Ring Thickness Pixels` configure objective emphasis independently of entity type.

Fighters use small triangles oriented by player-relative planar velocity. Capital ships use larger
solid hexagons, turrets use squares, and allegiance determines core colour. Objective status keeps
the normal entity glyph and adds a gold ring. Normal contacts have no shader glow; the radar
structure retains a separate emissive/halo treatment.

### Editor showcase

`EUW_Radar3DShowcase` is an editor-only harness in `SbxUIExperiments`. It feeds deterministic
synthetic frames into the same production `SRadarWidget`; it is not a separate radar implementation.
See [the showcase README](Source/SbxUIExperiments/Private/Radar3D/README.md) for launch and benchmark
instructions.
