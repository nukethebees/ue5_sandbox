# RDG Heatmap architecture

`UHeatmapRDGWidget::set_grid` validates a contiguous row-major grid on the game thread and captures
a float snapshot for an enqueued render command. That command uploads an RDG structured buffer,
dispatches `FHeatmapRDGCS`, and writes a persistent `PF_R8G8B8A8` render target. One `SImage` samples
the target directly, avoiding readback and per-cell Slate elements.

The output allocation remains 512x512 while smaller grids use only the written UV region. Grid
validation, render-target creation, brush changes, and enqueueing are game-thread operations; the
snapshot belongs to the queued render command. The widget's transient `UPROPERTY` owns the render
target while the Slate brush is presentation-only. Normal operation has no render-thread flush or
synchronous GPU wait.

Benchmark submission, command latency, upload/compute work, and Slate painting separately. The
prototype intentionally omits dirty regions, sparse grids, persistent upload buffers, mapped memory,
and double buffering.

See [README.md](README.md) for the demo entry point.
