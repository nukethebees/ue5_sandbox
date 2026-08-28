# Heatmap benchmark

This benchmark compares the experimental RDG heatmap with `SHeatmap2D`, the existing Slate
custom-vertex implementation. Both receive the same deterministic, already-binned scalar grids.

The shared runner is available from the **Benchmark RDG vs Slate custom vertices** button in
`EUW_HeatmapRDGShowcase`, and from the editor commandlet:

```text
UnrealEditor-Cmd.exe Sandbox.uproject -run=HeatmapBenchmark -AllowCommandletRendering \
  -RenderOffscreen -unattended \
  -Resolutions=32,64,128,256,512 -Warmup=10 -Iterations=100 \
  -Output=Saved/Benchmarks/HeatmapBenchmark.csv
```

`-AllowCommandletRendering` is required, and `-NullRHI` must not be used; the RDG case requires a
real rendering backend. The commandlet returns a non-zero exit code for invalid arguments or output
failures.

## Reported stages

- `RDG/api_submission`: grid validation, snapshot copy, render-target lookup, and render-command
  enqueue on the game thread.
- `RDG/gpu_upload_compute`: GPU timestamp interval containing the structured-buffer upload,
  transitions, and compute dispatch.
- `Slate/api_submission`: ownership transfer into `SHeatmap2D` and paint-cache invalidation.
- `Slate/geometry_batches`: colour mapping, per-cell geometry generation, and custom-vertex batch
  construction performed by the Slate heatmap's paint cache.

Grid generation, first-use shader/resource setup, and warmup iterations are outside measured
samples. Results include minimum, median, p95, maximum, and sample count in microseconds.

GPU timing uses Unreal's absolute-time RHI queries and is emitted only on platforms that support
them. Query waits serialize measured samples inside the benchmark, but do not affect the normal
heatmap path. `HeatmapRDG.Render` remains available for confirmation in Unreal Insights.

The benchmark does not yet isolate Slate's final renderer/GPU duration. Its `geometry_batches`
stage captures the dominant per-cell CPU work, while measuring Slate GPU work fairly requires an
offscreen window and a separately bracketed Slate renderer submission.
