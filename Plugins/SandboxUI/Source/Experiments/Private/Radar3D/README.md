# RDG 3D Radar Experiment

This experiment proves a minimal animated 3D-data-to-Slate path without a level, world, actor,
component, or scene capture:

```text
SRadar3DWidget synthetic FRadar3DContact array
        |
        | game-thread animation and render-command snapshot
        v
RDG StructuredBuffer<FRadar3DContact>
        |
        | bounded instanced raster passes with a fixed camera
        v
512x512 UTextureRenderTarget2D ----> FSlateBrush / SImage
```

The raster shaders project the contacts and fixed radar geometry analytically. A fullscreen pass
draws the background, then bounded plane, line, stem, and marker primitives draw only the pixels
they cover. There is deliberately no scene, depth buffer, mesh, material, readback, interaction,
or gameplay integration.

The Slate widget keeps the transient render target alive, updates one contact during its visible
tick, and submits a small contact snapshot to the render thread. The renderer registers the
persistent texture with RDG, uploads the snapshot as a structured buffer, dispatches the shader,
and leaves the texture ready for Slate sampling.

To view the experiment, show plugin content in the Content Browser, open `SandboxUI/Examples`,
right-click `EUW_Radar3DShowcase`, and select **Run Editor Utility Widget**.
Use the **Contacts** control to display between 1 and 256 deterministic contacts in the single
radar view.

The showcase also has a short contact-scaling benchmark. It reports game-thread submission and GPU
upload/raster timing for 1 through 256 contacts. See
`Private/Benchmarks/Radar3D/README.md` for the commandlet form and exact measurement boundaries.
