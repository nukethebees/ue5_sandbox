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

Unreal 5.8 enforces that global shader sources include `Platform.ush`, even though this shader uses
none of its definitions. The include is therefore guarded by `SANDBOX_STANDALONE_DXC`, the only
compatibility shim. Both paths still compile the same algorithm and real entry point from the same
authoritative file.

`native/shaders` registers one CTest. Its test-specific CMake runner invokes DXC with:

```text
dxc.exe -T cs_6_0 -E render_heatmap_cs -DSANDBOX_STANDALONE_DXC=1 -WX \
  -Fo <build>/HeatmapRDG.dxil \
  Plugins/SandboxUI/Shaders/Private/HeatmapRDG/HeatmapRDG.usf
```

Run it independently of Unreal with:

```powershell
cmake --preset native
ctest --test-dir out/build/native -R '^heatmap-rdg-dxc-test$' --output-on-failure
```

The test fails with a specific `DXC missing` message if the executable found during configuration
is unavailable. DXC's diagnostics are included verbatim when include resolution or HLSL
compilation fails. The test uses DXC 1.9.0.5402 installed at
`C:\tools\dxc\dxc_2026_07_29\bin\x64\dxc.exe` on this machine.

Three post-implementation runs used the exact CTest command above. Wall-clock time includes CTest,
the CMake test runner, DXC process startup, shader compilation, and writing the DXIL output.

| Run | Result | CTest test time | Process wall clock |
| --- | --- | ---: | ---: |
| 1 | Pass | 0.04 s | 0.079 s |
| 2 | Pass | 0.04 s | 0.069 s |
| 3 | Pass | 0.04 s | 0.071 s |

The representative median is **0.071 s**. DXC does not use a persistent compiler cache in this
test; all three runs launch a new process and produce a new DXIL file. The source and executable
were warm in the operating-system file cache after the earlier implementation check.

## Comparison and recommendation

| Path | Representative median | Absolute time saved | Relative speedup |
| --- | ---: | ---: | ---: |
| Unreal `HeatmapBenchmark` | 25.337 s | - | 1x |
| Standalone DXC CTest | 0.071 s | 25.266 s | 357x |

The timings are intentionally end-to-end but are not perfectly apples-to-apples. The Unreal path
starts the editor and D3D12 RHI, checks/compiles engine and project shaders, creates RDG resources,
dispatches the shader 550 times including warmups, takes serialized GPU timestamps for 500
measured dispatches, benchmarks the Slate alternative, and shuts down. The DXC path compiles the
single authoritative entry point once and exits. The comparison answers the practical question of
how long a developer waits for source/entry-point validation, not which compiler is faster.

No Unreal test or benchmark was removed. The existing commandlet retains coverage of global-shader
registration, Unreal virtual shader mapping, parameter layout and binding, RDG upload/UAV setup,
real D3D12 dispatch, and GPU execution. The standalone test adds cheap validation that the shader
is valid direct-DXC HLSL, the production entry point exists, `cs_6_0` compilation succeeds without
warnings, and the shader does not accidentally acquire an Unreal-only source dependency.

Neither path checks output pixels against expected heatmap colours. A D3D12 execution harness would
be required for deterministic algorithm-output coverage, but that is not justified for this first
proof: it would duplicate substantial resource and dispatch setup already covered by Unreal. The
compile-only test is worth keeping and the pattern is worth extending selectively to other small,
self-contained shaders. Further candidates should get one test per authoritative entry point and
should not trigger a general shader framework; shaders whose value is primarily Unreal integration
should remain in Unreal.
