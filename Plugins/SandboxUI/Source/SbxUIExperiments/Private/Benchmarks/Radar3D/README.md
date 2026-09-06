# Radar3D benchmark

This benchmark exercises the production radar renderer at a fixed 512x512 output size while
scaling the synthetic contact array. It is available from the **Benchmark RDG contact scaling**
button in `EUW_Radar3DShowcase`, and from the editor commandlet:

```text
UnrealEditor-Cmd.exe Sandbox.uproject -run=Radar3DBenchmark -AllowCommandletRendering \
  -RenderOffscreen -unattended \
  -ContactCounts=32,128,256,512 -Warmup=10 -Iterations=100 \
  -Output=Saved/Benchmarks/Radar3DBenchmark.csv
```

`-AllowCommandletRendering` is required, and `-NullRHI` must not be used.

## Reported stages

- `collection_transform`: deterministic presentation-instance preparation.
- `api_submission`: frame selection and render-command enqueue on the game thread.
- `gpu_upload_raster`: GPU timestamp interval containing the structured-buffer upload,
  transitions, and instanced raster passes.

Render-target creation, first-use setup, synchronization, and warmup iterations are outside
measured samples. Results include minimum, median, p95, maximum, and sample count in microseconds.
GPU timing is emitted only when absolute-time RHI queries are supported.
