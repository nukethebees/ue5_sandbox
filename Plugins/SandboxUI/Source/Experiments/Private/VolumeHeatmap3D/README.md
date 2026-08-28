# RDG 3D Volume Heatmap Experiment

This experiment visualizes a dense scalar field without a world, actors, scene capture, or one
primitive per voxel:

```text
x-fastest dense CPU float volume
        |
        | dirty-only immutable snapshot
        v
RDG StructuredBuffer<float>
        |
        | one instanced draw of back-to-front view-aligned slices
        v
manual trilinear sampling + emission/absorption compositing
        |
        v
512x512 UTextureRenderTarget2D ----> FSlateBrush / SImage
```

The slice shader clips each plane to the `[-1, 1]` volume, samples eight neighbouring voxels, and
uses slice-length-corrected alpha. A bounding cube and colour-coded axes make the view orientation
clear. The showcase provides deterministic Gaussian-cloud and hollow-shell fields, grid choices
from `16³` to `128³`, 8–256 slices, camera yaw/pitch, and density scaling.

Like Scatter3D, the widget waits two Slate ticks before its first render. This is required for a
static render target: a constructor-time render can be replaced during the first Slate resource
registration and leave a black image even though the RDG work succeeded. After startup, the widget
does not tick or render until a control is committed.

The v1 path deliberately uploads the whole dense volume on every data change. It does not retain a
Texture3D, perform partial uploads, ray march, light the volume, or create voxel geometry. The
benchmark separates API submission (including the CPU snapshot) from GPU upload plus raster time;
see `Private/Benchmarks/VolumeHeatmap3D/README.md`.

To view it, show plugin content, open `SandboxUI/Examples`, right-click
`EUW_VolumeHeatmap3DShowcase`, and select **Run Editor Utility Widget**.
