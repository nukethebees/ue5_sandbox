# Standalone DXC shader tests

## Purpose and architecture

This is a deliberately small compile-time validation path for authoritative project HLSL. It does
not execute shaders, emulate Unreal's shader environment, or provide a general shader framework.

Previously, ordinary source validity was discovered through Unreal startup and one of three
boundaries:

- global-shader registration followed by an editor, commandlet, or render test;
- material compilation through Unreal; or
- twelve `FPaths::FileExists(GetShaderSourceFilePath(...))` assertions in
  `SandboxShaders.UnitTests.ShaderInfrastructure.InitialisesModulesAndShaderMapping`.

The new path registers direct DXC invocations as CTests. Every test compiles the production `.usf`
entry point or `.ush` library in place, with warnings as errors, and writes only DXIL build output.
DXC is a required developer prerequisite: `cmake --preset native` fails during configuration when
`dxc` or `dxc.exe` cannot be found through normal CMake program discovery.

Each registration supplies its own target profile. Compute, vertex, and pixel entries currently use
`cs_6_0`, `vs_6_0`, or `ps_6_0`; entry-point-free HLSL libraries use `lib_6_3`. The helper does not
make `cs_6_0` a default for future shaders.

Unreal 5.8 requires global shader sources to include `Platform.ush`, even where these shaders use no
definitions from it. `SANDBOX_STANDALONE_DXC` guards only that mandatory include. SandboxShaders
libraries with Unreal virtual includes select the same authoritative dependency through a relative
path under standalone DXC. No algorithm source is copied. Direct compilation also exposed two
existing implicit float3-to-float2 conversions; explicit `.xy` selection preserves their behavior
and makes the library tests clean under `-WX`.

## Native test inventory

There are **41** native shader tests: **25 production entry points** and **16 HLSL libraries**.
HeatmapRDG was the original proof; the audit added the other 40 tests.

| Source family | CTest names / entry points | Profile | Count |
| --- | --- | --- | ---: |
| HeatmapRDG | `heatmap-rdg-dxc-test` / `render_heatmap_cs` | `cs_6_0` | 1 |
| Radar | `radar-{plane-vs,plane-ps,structure-vs,stem-vs,line-ps,glyph-vs,glyph-ps}-dxc-test` | matching `vs_6_0` / `ps_6_0` | 7 |
| Scatter3D | `scatter-3d-{background-vs,background-ps,frame-vs,frame-ps,sample-vs,sample-ps}-dxc-test` | matching `vs_6_0` / `ps_6_0` | 6 |
| VolumeHeatmap3D | `volume-heatmap-3d-{background-vs,background-ps,slice-vs,slice-ps,frame-vs,frame-ps}-dxc-test` | matching `vs_6_0` / `ps_6_0` | 6 |
| EntityOverlay | `entity-overlay-{vs,ps}-dxc-test` | matching `vs_6_0` / `ps_6_0` | 2 |
| NebulaDensity | `nebula-density-dxc-test` / `generate_nebula_density_cs` | `cs_6_0` | 1 |
| SpaceEnergyField | `space-energy-field-dxc-test` / `render_space_energy_field_cs` | `cs_6_0` | 1 |
| SparkExpansion | `spark-expansion-dxc-test` / `expand_spark_bursts_cs` | `cs_6_0` | 1 |
| SandboxShaders libraries | `<lowercase filename>-library-dxc-test` for ProceduralNoise, CelestialBackdrop, ConstructionSpawn, EnergyBeam, EnergyShield, EngineExhaust, NebulaBackdrop, NebulaCommon, NebulaVolume, PlanetAtmosphere, RadarDisplay, RaymarchedAnomaly, ShieldImpact, TacticalScan, VertexRipple, and WarpField | `lib_6_3` | 16 |

Configure and run the complete standalone path without Unreal:

```powershell
cmake --preset native
ctest --preset native-tests -L '^native-shader$' --output-on-failure
```

CTest launches DXC directly, so missing entry points, include failures, warnings, and compiler errors
appear in the normal failing-test output. The effective invocation for each registration is:

```text
dxc.exe -T <profile> [-E <entry-point>] -DSANDBOX_STANDALONE_DXC=1 -WX \
  -Fo <native-build>/<test-name>.dxil <authoritative-source>
```

The configure-time failure was checked in a separate build directory by excluding the installed DXC
directory from CMake discovery:

```powershell
cmake --preset native -B out/build/native-no-dxc `
  "-DCMAKE_IGNORE_PATH=C:/tools/dxc/dxc_2026_07_29/bin/x64"
```

Configuration stopped with:

```text
Could not find SANDBOX_DXC_EXECUTABLE using the following names: dxc, dxc.exe
```

## HeatmapRDG proof baseline

HeatmapRDG was selected first because it is the smallest exercised global shader: one compute entry
point, one structured-buffer input, one UAV output, and no scene, material, Niagara, or
vertex-factory dependencies. Before the proof, its authoritative HLSL was compiled as an Unreal
global shader and exercised by the real-RHI `HeatmapBenchmark` commandlet. That validates Unreal
registration, parameter binding, RDG resources, dispatch, and GPU execution, but it does not compare
output pixels with expected colours.

The Unreal baseline was captured on 2026-09-20 before changing the shader or test path. The
DebugGame editor was built first with:

```powershell
cmake --build --preset debug-game --target editor
```

The canonical benchmark target is:

```powershell
cmake --build --preset debug-game --target heatmap-benchmark
```

That target initially encountered a recursive Ninja prerequisite-build stall. After the editor
build completed, the timed runs used its exact jobserver-wrapped commandlet:

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
iterations per resolution for both the RDG and Slate implementations. Wall time includes
jobserver/client and Unreal process startup, RHI and DDC setup, the benchmark, and shutdown.

| Run | Cache state | Result | Wall clock | Approx. setup before report | Shader activity observed |
| --- | --- | --- | ---: | ---: | --- |
| 1 | Cold after editor rebuild | Pass | 80.453 s | 69 s | 29 local shader workers; missing engine and project material shadermaps compiled; SM5/SM6 autogen checked |
| 2 | Warm | Pass | 25.337 s | 19 s | Shader workers started; SM5/SM6 autogen checked; six engine debug material shadermaps compiled |
| 3 | Warm | Pass | 20.460 s | 16 s | Shader workers started; SM5/SM6 autogen checked; six engine debug material shadermaps compiled |

The representative Unreal median is **25.337 s**. The original single-entry DXC measurements were
0.079, 0.069, and 0.071 s process wall time, for a **0.071 s** median. They launched a new DXC
process, wrote a fresh DXIL file, and included the CMake test runner used by the original proof. The
source and executable were warm in the operating-system file cache after implementation checks.

| Validation path | Representative median | Absolute latency saved | Relative latency reduction |
| --- | ---: | ---: | ---: |
| Existing Unreal Heatmap workflow | 25.337 s | - | 1x |
| Standalone Heatmap DXC test | 0.071 s | 25.266 s | 357x |

The **357x figure is specifically the reduction in end-to-end developer latency for Heatmap source
and entry-point validation**. It is not a claim that DXC executes equivalent work 357 times faster.
The Unreal workflow starts the editor and D3D12 RHI, checks or compiles engine/project shaders,
creates RDG resources, dispatches Heatmap 550 times including warmups, records 500 serialized GPU
timings, benchmarks the Slate alternative, and shuts down. The DXC path compiles one authoritative
entry point once and exits.

Baseline environment:

- Unreal Engine 5.8.0, DebugGame editor, D3D12, SM6
- Windows 11 25H2, build 10.0.26200.9457
- AMD Ryzen 9 9950X3D, 16 cores / 32 logical processors
- NVIDIA GeForce RTX 5090, driver 610.88
- 128 GB physical memory reported by Unreal
- DXC 1.9.0.5402 at `C:\tools\dxc\dxc_2026_07_29\bin\x64\dxc.exe`
- Repository commit before the Heatmap implementation: `db03e9aa30f8665a9b797e82aa463ab753880410`

## Audit and Unreal test reduction

| Shader/system | Standalone compile? | Hoisted now? | Unreal test reduced? | Unreal coverage retained | Reason |
| --- | --- | --- | --- | --- | --- |
| HeatmapRDG | Yes, one compute entry | Yes | No | Global-shader registration, RDG binding/dispatch, GPU timing, Slate comparison | Smallest original proof; compile-only DXC adds source validity but cannot replace execution coverage |
| Radar | Yes, 4 vertex and 3 pixel entries | Yes | No | Widget/data tests and Radar3D commandlet benchmark | Only mandatory `Platform.ush` dependency; every registered entry compiles directly |
| Scatter3D | Yes, 3 vertex and 3 pixel entries | Yes | No | Slate/widget integration and Scatter3D benchmark | Self-contained global-shader HLSL |
| VolumeHeatmap3D | Yes, 3 vertex and 3 pixel entries | Yes | No | Slate/widget integration and VolumeHeatmap3D benchmark | Self-contained global-shader HLSL |
| EntityOverlay | Yes, vertex and pixel entries | Yes | No | Frame-store/collector tests and EntityOverlay benchmark | Self-contained global-shader HLSL |
| SpaceEnergyField | Yes, one compute entry plus ProceduralNoise | Yes | One existence assertion removed | Unreal global-shader registration, RDG resources, real-RHI dispatch, material/showcase integration | Relative standalone include is a narrow replacement for the Unreal virtual include |
| NebulaDensity and Nebula libraries | Yes, one compute entry and three libraries | Yes | No | Unreal registration, texture/RDG integration, material assets | Sources compile without an engine emulation layer |
| Other SandboxShaders material libraries | Yes, as `lib_6_3` | Yes | Eleven existence assertions removed | Plugin mapping, material assets, material recompilation, showcase actors | Library compilation type-checks real functions and nested includes more strongly than file existence |
| SparkExpansion | Yes, one compute entry | Yes | No | Spark buffer integration, vertex factory, real render-pixel tests and benchmark | Compute source is portable; rendering integration remains Unreal-specific |
| GpuStarfield, Spark, and SandboxISMC vertex factories | No practical standalone compile | No | No | All Unreal vertex-factory/render coverage | Depend on engine private vertex-factory headers, types, macros, and scene integration |
| SandboxGpuTutorials `.ush` lessons | Technically compilable as libraries | No | No | Material asset domain, include/function contracts, CPU/GPU parameters | Existing tests validate material assets; extra direct compilation would not replace an Unreal assertion |
| USFLoader | TestDummy is technically compilable; resolver is not portable | No | No | All path validation, dependency reporting, and transient material compilation | `FString`, shader directory mappings, `GetShaderSourceFilePath`, and material expressions are the behavior under test |
| Niagara and scene/material-template paths | No practical low-risk compile | No | No | Existing Unreal integration | Would require recreating Unreal shader globals or engine header machinery |
| Legacy project `.ush` snippets | No current shader smoke-test boundary | No | No | Existing material/project usage | Includes stale absolute/engine-specific paths and is unrelated to the tested plugin shader systems |

No Unreal automation test case was deleted. Exactly these **12 source-file existence assertions**
were removed from `InitialisesModulesAndShaderMapping`: Energy shield, Space field, Shield impact,
Vertex ripple, Radar display, Tactical scan, Warp field, Raymarched anomaly, Engine exhaust, Planet
atmosphere, Construction spawn, and Energy beam.

The same test still proves both modules load, the plugin is discoverable, the
`/Plugin/SandboxShaders` mapping exists, and the mapping targets the plugin's real shader directory.
`ShaderInfrastructure.LoadsMaterialsAndComposedShowcase`,
`MaterialCompilation.CompilesMaterialsAndDispatchesSpaceField`, all USFLoader path/material tests,
the GPU tutorial material tests, real-RHI/RDG tests, render-pixel tests, and commandlet benchmarks
remain unchanged.

## Post-audit measurements

The focused Unreal timing command, used before and after removing the assertions, was:

```powershell
$timer = [Diagnostics.Stopwatch]::StartNew()
& "$env:LOCALAPPDATA\NukeTheBees\jobserver\bin\jobserver.exe" run `
  --name "SandboxShaders shader mapping run <n>" `
  --kind unreal-test `
  --worktree "C:\Users\matthew\source\repos\nukethebees\wt\dev2" `
  --shared machine `
  --shared unreal-build/8da32d479a17c0fbbf96cfbbcf14b092a6fd0bb3454eb8e1dadbb0c653e88d0b `
  -- "C:\dev\UE5.8.0\Engine\Binaries\Win64\UnrealEditor-Win64-DebugGame-Cmd.exe" `
  "C:\Users\matthew\source\repos\nukethebees\wt\dev2\Sandbox.uproject" `
  "-ExecCmds=Automation RunTests SandboxShaders.UnitTests.ShaderInfrastructure.InitialisesModulesAndShaderMapping; Quit" `
  -nullrhi -unattended -nop4 -nosplash -nosound -stdout -ddc=NoZenLocalFallback `
  "-LocalDataCachePath=C:\Users\matthew\source\repos\nukethebees\wt\dev2\out\build\debug-game\local-derived-data-cache"
$timer.Stop()
$timer.Elapsed.TotalSeconds
```

| State | Run 1 | Run 2 | Run 3 | Median | Result / tests |
| --- | ---: | ---: | ---: | ---: | --- |
| Before, 12 source assertions present | 15.337 s | 15.344 s | 15.327 s | 15.337 s | Pass, 1 test |
| After, source assertions removed | 15.323 s | 15.333 s | 15.318 s | 15.323 s | Pass, 1 test |

The apparent 0.014 s reduction is below useful measurement resolution. Editor startup, module
loading, asset-registry work, shader worker startup, and test discovery dominate; the actual test
body took about 0.010 s in both sets. No Unreal speedup is claimed.

Three timed native suite runs used the exact CTest command shown above after an initial validation
run:

| Run | Tests | Result | CTest reported time | CTest process wall clock |
| --- | ---: | --- | ---: | ---: |
| 1 | 41 | Pass | 1.06 s | 1.091 s |
| 2 | 41 | Pass | 1.08 s | 1.103 s |
| 3 | 41 | Pass | 1.09 s | 1.119 s |

The representative process-wall median is **1.103 s** for all 41 tests. It includes CTest and 41
fresh DXC process launches; DXC has no persistent compiler cache in this setup. These runs were warm
in the operating-system file cache. The meaningful migration metrics are therefore 25 production
entry points and 16 libraries validated natively in about 1.1 seconds, with 12 low-value Unreal
existence assertions removed and all Unreal-specific integration coverage retained.

Final Unreal validation rebuilt the changed automation test and exercised the retained material/RDG
and Spark execution boundaries:

```powershell
cmake --build --preset debug-game --target editor
ctest --test-dir out/build/debug-game `
  -R '^SandboxShaders\.MaterialCompilation$' --output-on-failure
ctest --test-dir out/build/debug-game `
  -R '^Sandbox\.SparkRenderTests$' --output-on-failure
```

The editor build passed, `SandboxShaders.MaterialCompilation` passed in 20.34 s, and
`Sandbox.SparkRenderTests` passed in 20.36 s. The focused post-change shader-mapping automation test
also passed on all three timing runs above.

## Recommendation and limits

This pattern is worth extending when a production entry point or HLSL library can compile from its
authoritative source with only a narrow engine-include guard. It should not be extended to vertex
factories, Niagara, scene-texture shaders, or material-template-dependent code by faking Unreal's
environment.

Compile-only tests do not validate numerical output. Deterministic standalone output checks for
HeatmapRDG, SpaceEnergyField, NebulaDensity, or SparkExpansion would require a D3D12 device,
resources, pipeline state, dispatch, and readback. That harness is not justified by this audit:
Unreal already owns the relevant RDG/render integration, and Spark has render-pixel coverage. Add a
narrow D3D12 execution test only if a future shader has important algorithmic output that cannot be
validated adequately through existing Unreal integration.
