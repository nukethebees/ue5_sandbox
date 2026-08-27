# Radar3D benchmark

This benchmark exercises the experimental Radar3D renderer at a fixed 512x512 output size while
scaling the synthetic contact array. It is available from the **Benchmark RDG contact scaling**
button in `EUW_Radar3DShowcase`, and from the editor commandlet:

```text
UnrealEditor-Cmd.exe Sandbox.uproject -run=Radar3DBenchmark -AllowCommandletRendering \
  -RenderOffscreen -unattended \
  -ContactCounts=1,4,16,64,256 -Warmup=10 -Iterations=100 \
  -Output=Saved/Benchmarks/Radar3DBenchmark.csv
```

`-AllowCommandletRendering` is required, and `-NullRHI` must not be used. Contact counts are capped
at 256 because the experiment's shader evaluates every contact for every output pixel.

## Reported stages

- `api_submission`: contact snapshot allocation/copy and render-command enqueue on the game thread.
- `gpu_upload_compute`: GPU timestamp interval containing the structured-buffer upload,
  transitions, and full compute dispatch.

Synthetic data generation, render-target creation, first-use setup, synchronization, and warmup
iterations are outside measured samples. Results include minimum, median, p95, maximum, and sample
count in microseconds. GPU timing is emitted only when absolute-time RHI queries are supported.
