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
512x512 UTextureRenderTarget2D ----> FSlateBrush / SImage
```

The production renderer draws a transparent hex plane, honeycomb, structure, stems, and glyphs in
four bounded raster passes. The showcase supplies deterministic glyph and emphasis variants.

To view the experiment, show plugin content in the Content Browser, open `SandboxUI/Examples`,
right-click `EUW_Radar3DShowcase`, and select **Run Editor Utility Widget**.
Use the **Contacts** control to display between 1 and 512 deterministic contacts in the single
radar view.

The showcase also has a short contact-scaling benchmark. It reports game-thread submission and GPU
collection, submission, and upload/raster timing. See
`Private/Benchmarks/Radar3D/README.md` for the commandlet form and exact measurement boundaries.
