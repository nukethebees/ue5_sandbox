# 3D radar showcase

This editor-only utility exercises the production `SandboxUI/Radar` widget without a level or
world. It feeds synthetic frames into the same `SRadarWidget` used by `SpaceGame`; no radar renderer
is implemented in `SbxUIExperiments`.

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

Production integration and authoring are documented in the plugin-level
[SandboxUI README](../../../../README.md).

## Showcase

To view the experiment, show plugin content in the Content Browser, open `SandboxUI/Examples`,
right-click `EUW_Radar3DShowcase`, and select **Run Editor Utility Widget**.
Use the **Contacts** control to display between 1 and 512 deterministic contacts in the single
radar view.

The showcase also has a short contact-scaling benchmark. It reports game-thread submission and GPU
collection, submission, and upload/raster timing. See the
[benchmark README](../Benchmarks/Radar3D/README.md) for the commandlet form and exact measurement
boundaries.
