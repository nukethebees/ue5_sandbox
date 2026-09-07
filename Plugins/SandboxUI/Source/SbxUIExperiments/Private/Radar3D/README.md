# Production radar showcase

This editor utility exercises the runtime `SandboxUI/Radar` widget without a level or world:

```text
Synthetic FRadarFrame triple buffer -> SRadarWidget
        |
        | game-thread animation and render-command snapshot
        v
RDG StructuredBuffer<FRadarInstance>
        |
        | bounded instanced raster passes with a fixed camera
        v
1024x1024 UTextureRenderTarget2D ----> FSlateBrush / SImage
```

The production renderer draws a transparent hex plane, honeycomb, structure, stems, and glyphs in
four bounded raster passes. The showcase supplies deterministic glyph and emphasis variants.

## Using the production radar

`UShipHudWidget` expects two named widgets in its Widget Blueprint:

- `radar_host`: a `NativeWidgetHost` into which the runtime `SRadarWidget` is inserted.
- `radar_background`: a `Border` occupying the same slot. Its brush is made transparent at runtime.

Both must be direct children of a `CanvasPanel`. At runtime their authored Canvas Panel slot layout
is replaced with a bottom-right anchor, `640 x 640` design-unit size, and `32` design-unit viewport
margin. Unreal's application DPI scale is then applied; for example, a DPI scale of `0.67` displays
the radar at approximately `429 x 429` screen pixels. Resizing the green `radar_host` rectangle in
the Widget Blueprint designer alone therefore does not change the in-game size.

The radar's presentation settings are edited on the level's `ATestBatchOrchestrator` under
**Sandbox > Presentation > Radar**:

- `Enabled` controls whether the radar is populated and shown.
- `Combat Range`, `Tactical Range`, `Maximum Range`, and `Combat Display Radius` configure the fixed
  nonlinear distance mapping.
- `Grid Opacity` and the four cell-radius settings configure honeycomb visibility and density.
- `Glyph Size Scale` scales all contact cores without changing the radar footprint.
- `Objective Size Multiplier`, `Objective Ring Padding Pixels`, and
  `Objective Ring Thickness Pixels` configure objective emphasis independently of entity type.

Fighters use small triangles oriented by their player-relative planar velocity. Capital ships use
larger solid hexagons, turrets use squares, allegiance determines the core colour, and objective
status adds a gold ring around the normal entity glyph. Normal contacts have no shader glow; the
radar structure retains its separate emissive/halo treatment.

The in-game radar footprint and bottom-right margin are currently fixed by
`configure_radar_canvas_slot` in `ShipHudWidget.cpp`; there are no editor properties for these two
layout values yet. Exposing them would only require passing two presentation values into the HUD
layout and would not change the radar renderer or its GPU cost.

## Showcase

To view the experiment, show plugin content in the Content Browser, open `SandboxUI/Examples`,
right-click `EUW_Radar3DShowcase`, and select **Run Editor Utility Widget**.
Use the **Contacts** control to display between 1 and 512 deterministic contacts in the single
radar view.

The showcase also has a short contact-scaling benchmark. It reports game-thread submission and GPU
collection, submission, and upload/raster timing. See
`Private/Benchmarks/Radar3D/README.md` for the commandlet form and exact measurement boundaries.
