# HeatmapRDG standalone DXC proof of concept

## Why this shader

`Plugins/SandboxUI/Shaders/Private/HeatmapRDG/HeatmapRDG.usf` is the smallest existing shader
with an exercised Unreal path. It has one compute entry point, one structured-buffer input, one UAV
output, and no scene, material, Niagara, or vertex-factory dependencies. The only engine include is
`/Engine/Public/Platform.ush`; the shader does not use anything from it.

Before this proof of concept, the authoritative HLSL was compiled as an Unreal global shader and
exercised by the real-RHI `HeatmapBenchmark` commandlet. That path validates Unreal shader
registration, RDG resources and binding, dispatch, and GPU execution. It does not validate output
pixels against expected colours.

## Unreal baseline

The baseline was captured on 2026-09-20 before changing the shader or test path. The DebugGame
editor was built first with:

```powershell
cmake --build --preset debug-game --target editor
```

The canonical benchmark command is:

```powershell
cmake --build --preset debug-game --target heatmap-benchmark
```

The preset target initially encountered a recursive Ninja prerequisite-build stall. After building
the `editor` target successfully, each timed run used the exact jobserver-wrapped commandlet from
the `heatmap-benchmark` target:

```powershell
& "$env:LOCALAPPDATA\NukeTheBees\jobserver\bin\jobserver.exe" run `
  --name "Baseline HeatmapRDG Unreal run <n>" `
  --kind benchmark `
  --worktree "C:\Users\matthew\source\repos\nukethebees\wt\dev2" `
  --exclusive machine --exclusive benchmark `
  --shared unreal-build/8da32d479a17c0fbbf96cfbbcf14b092a6fd0bb3454eb8e1dadbb0c653e88d0b `
  -- "C:\dev\UE5.8.0\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe" `
  "C:\Users\matthew\source\repos\nukethebees\wt\dev2\Sandbox.uproject" `
  -run=HeatmapBenchmark -Resolutions=32,64,128,256,512 -Warmup=10 -Iterations=100 `
  -Output=Saved/Benchmarks/HeatmapBenchmark.csv -AllowCommandletRendering `
  -RenderOffscreen -unattended -nop4 -nosplash -nosound -stdout
```

Each run is one commandlet workload covering five resolutions, with 10 warmup and 100 measured
iterations per resolution for both the RDG and Slate implementations. Wall-clock timing includes
jobserver/client and Unreal process startup, RHI and DDC setup, the benchmark, and shutdown.

| Run | Cache state | Result | Wall clock | Approx. engine/setup before report | Shader activity observed |
| --- | --- | --- | ---: | ---: | --- |
| 1 | Cold after editor rebuild | Pass | 80.453 s | 69 s | 29 local shader workers; missing engine and project material shadermaps compiled; SM5/SM6 autogen checked |
| 2 | Warm | Pass | 25.337 s | 19 s | Shader workers started; SM5/SM6 autogen checked; six engine debug material shadermaps compiled |
| 3 | Warm | Pass | 20.460 s | 16 s | Shader workers started; SM5/SM6 autogen checked; six engine debug material shadermaps compiled |

The representative median is **25.337 s**. The cold result is retained because first-use shader/DDC
work is real, but it is not used as the representative timing. Startup and setup dominate: the
benchmark report appeared about six seconds after the final startup scripts in all three runs.

### Baseline environment

- Unreal Engine 5.8.0, DebugGame editor, D3D12, SM6
- Windows 11 25H2, build 10.0.26200.9457
- AMD Ryzen 9 9950X3D, 16 cores / 32 logical processors
- NVIDIA GeForce RTX 5090, driver 610.88
- 128 GB physical memory reported by Unreal
- Repository commit before implementation: `db03e9aa30f8665a9b797e82aa463ab753880410`

## Standalone DXC path

To be completed after the standalone test is implemented and measured.

## Comparison and recommendation

To be completed after the standalone test is implemented and measured.
