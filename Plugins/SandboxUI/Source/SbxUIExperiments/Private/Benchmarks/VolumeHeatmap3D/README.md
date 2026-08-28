# Volume heatmap benchmark

This benchmark separates game-thread API submission (including the immutable dense-volume copy)
from GPU upload plus slice rasterization. It sweeps `16³` through `128³` at 96 slices, then 16
through 256 slices at `64³`. The output is 512×512.

Run with a real rendering backend:

```text
UnrealEditor-Win64-DebugGame-Cmd.exe Sandbox.uproject -run=VolumeHeatmap3DBenchmark -AllowCommandletRendering -RenderOffscreen -d3d12 -unattended -nop4 -stdout
```

The default CSV is `Saved/Benchmarks/VolumeHeatmap3DBenchmark.csv`.
