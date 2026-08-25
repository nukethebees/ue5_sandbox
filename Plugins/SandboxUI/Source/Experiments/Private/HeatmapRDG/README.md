# RDG Heatmap Experiment

This experiment proves a bulk CPU-to-GPU heatmap path that uses RDG for visualization and a
single Slate image for presentation. The caller remains responsible for binning gameplay data
into a dense, row-major scalar grid.

```text
caller-owned FHeatmapGrid
        |
        | game-thread validation and snapshot
        v
render-command-owned TArray<float>
        |
        | RDG structured-buffer upload
        v
StructuredBuffer<float> ----> FHeatmapRDGCS ----> PF_R8G8B8A8 render target
                                                      |
                                                      | persistent RHI texture
                                                      v
                                                FSlateBrush / SImage
```

## CPU to GPU flow

`UHeatmapRDGWidget::set_grid` accepts contiguous floats plus explicit width and height. It checks
positive dimensions and verifies the element count with an overflow-safe multiplication. Invalid
input is logged and leaves the previous output untouched.

Accepted values are copied into a snapshot captured by an enqueued render command. On the render
thread, `CreateStructuredBuffer` exposes that snapshot as an RDG structured buffer. The upload is
marked `NoCopy` because the command owns the array until the graph has executed; it does not mean
that the CPU data bypasses the RHI upload.

The graph registers the widget's persistent `UTextureRenderTarget2D` RHI texture as an external
RDG texture. One 8x8-thread-group global compute pass reads one float per cell, clamps it to
`[0, 1]`, maps it through a fixed colour ramp, and writes one `PF_R8G8B8A8` output pixel per cell.
The graph leaves the external texture in shader-resource access for Slate.

The render target is recreated only when grid dimensions change. Same-sized updates reuse the
texture and only submit a new snapshot, upload, and compute pass.

## Slate and editor utility presentation

`UHeatmapRDGWidget` is a native UMG widget backed by one `SImage`. Its brush references the
transient render target directly, so Slate samples the GPU result without readback. Painting is
constant with respect to cell count: it produces one image element, not one element per cell.
Nearest filtering keeps individual cells visible when the widget is enlarged.

The example frontend is `UHeatmapRDGEditorUtilityWidget`, a native C++ editor utility widget. The
module registers it as a nomad editor tab and builds its controls and layout directly in Slate, so
there is no Widget Blueprint or content asset to maintain.

To view the demo:

1. Open **Tools > Developer Tools > Miscellaneous** in the Unreal Editor.
2. Select **RDG Heatmap Experiment**.
3. Use the size buttons to submit 32x32 through 512x512 grids, and switch between the deterministic
   hotspot and gradient/checker patterns.

The widget submits a deterministic 128x128 multi-hotspot pattern on first construction. The
pattern controls exercise same-size content updates, while the size controls also exercise output
texture recreation. C++ callers can call `set_grid` with their own `FHeatmapGrid`, or call
`generate_demo_grid` with another size. `Experiments` is an editor-only module and is not loaded
by normal game or shipping targets.

## Threading and lifetime

- Grid validation, render-target creation, brush changes, and render-command enqueue happen on
  the game thread.
- The captured float snapshot is owned by the queued command and is moved into the RDG submission
  on the render thread.
- `UHeatmapRDGWidget` keeps the render-target UObject alive with a transient `UPROPERTY`; the brush
  is only a presentation reference.
- Unreal queues texture initialization, heatmap work, Slate rendering, and eventual texture
  resource release on the render thread. Command ordering prevents a resize or UObject teardown
  from releasing an RHI resource ahead of already-enqueued work.
- No render-thread flush, synchronous GPU wait, or GPU-to-CPU readback occurs in the normal path.

## Benchmark notes

The useful comparison sizes are 32x32, 64x64, 128x128, 256x256, and 512x512. Keep these timing
boundaries separate when comparing against the existing Slate/custom-vertex heatmap:

- caller-side grid generation or update;
- `set_grid` validation, CPU snapshot allocation, and copy;
- render-command latency and RDG buffer/texture setup;
- the named `HeatmapRDG.Render` upload/compute GPU work;
- Slate paint and batch cost for the single image.

Likely first-version bottlenecks are the full CPU snapshot, full structured-buffer upload, render
target recreation when dimensions change, and one GPU dispatch per submitted update. This
experiment deliberately omits dirty regions, persistent upload buffers, mapped memory, double
buffering, sparse grids, and a benchmark harness.
