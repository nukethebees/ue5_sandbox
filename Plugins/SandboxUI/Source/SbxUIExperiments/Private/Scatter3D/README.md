# RDG 3D Scatter Experiment

This experiment renders deterministic 3D point clusters without a level, world, actors,
components, or scene capture:

```text
SScatter3DWidget synthetic FScatter3DPoint array
        |
        | dirty-only game-thread snapshot
        v
RDG StructuredBuffer<FScatter3DPoint>
        |
        | bounded frame primitives + depth-tested instanced point quads
        v
512x512 UTextureRenderTarget2D ----> FSlateBrush / SImage
```

The fixed-camera raster shader draws a dark background, a bounding cube, a floor grid, and
colour-coded X/Y/Z axes. A transient RDG depth texture gives the frame and circular point
billboards correct front-to-back occlusion. The visible result remains one persistent texture and
one Slate image.

The widget does not tick continuously. It temporarily ticks during startup, waits until Slate has
had an opportunity to paint and register the render-target resource, submits the initial render,
and then disables ticking. It renders again only after a committed point-count change, so an
unchanged scatter plot submits no CPU or GPU rendering work per tick. The example generates four
deterministic coloured clusters plus sparse outliers in a fixed `[-1, 1]` domain.

## Slate render-target initialization

A static render-target widget must not rely on a single render submitted from its `Construct`
function. At that point the `SImage` has not necessarily completed its first Slate paint/resource
registration. The RDG pass can successfully populate the `UTextureRenderTarget2D`, only for the
initial Slate/resource setup to leave the displayed image showing the render target's black clear.

The animated Radar3D experiment hides this ordering problem because it resubmits every tick. For a
static widget, keep ticking only long enough to pass the first paint, submit once, invalidate the
widget for paint, and then call `SetCanTick(false)`. This preserves dirty-only rendering without
depending on construction-time ordering.

Do not treat a successful shader compilation or GPU timing query as proof that the widget displays
visible output. When diagnosing a black image, read the render target back and inspect its pixel
range first. If it contains visible pixels, investigate Slate presentation and submission timing;
if it does not, investigate the RDG passes, pipeline state, projection, and render-target bindings.

To view the experiment, show plugin content in the Content Browser, open `SandboxUI/Examples`,
right-click `EUW_Scatter3DShowcase`, and select **Run Editor Utility Widget**. The **Points** control
accepts values from 1 through 65,536 and starts at 4,096.

The showcase also includes a point-scaling benchmark. It reports game-thread submission and GPU
upload/raster timing; see `Private/Benchmarks/Scatter3D/README.md` for the commandlet form and exact
measurement boundaries.
