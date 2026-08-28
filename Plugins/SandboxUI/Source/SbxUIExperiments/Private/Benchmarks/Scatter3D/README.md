# Scatter3D benchmark

This benchmark exercises the experimental Scatter3D renderer at a fixed 512x512 output size with
1, 64, 1,024, 16,384, and 65,536 points by default. Run it from the button in
`EUW_Scatter3DShowcase` or through the editor commandlet with a real graphics RHI:

```text
UnrealEditor-Cmd.exe Sandbox.uproject -run=Scatter3DBenchmark -AllowCommandletRendering \
  -RenderOffscreen -PointCounts=1,64,1024,16384,65536 -Warmup=20 -Iterations=100 \
  -Output=Saved/Benchmarks/Scatter3DBenchmark.csv
```

Do not use `-NullRHI`; it cannot compile or execute the graphics shader permutations being
measured.

The CSV reports these stages independently:

- `api_submission`: the game-thread `render` call, including allocation and copying of the point
  snapshot captured by the render command. It excludes point generation and render-thread work.
- `gpu_upload_raster`: timestamp-query duration around the RDG graph, including the structured
  buffer upload, background/frame passes, depth allocation, and instanced point raster pass. It
  excludes Slate painting and CPU waiting outside the query interval.

An unchanged `SScatter3DWidget` performs no render submission per tick. These measurements
represent the cost of changing its point data, not an idle-frame cost.
