# SandboxISMC

`SandboxISMC` is a Phase 1 rendering experiment, not a replacement for Unreal's
`UInstancedStaticMeshComponent`. It intentionally contains no collision, navigation, physics,
replication, foliage, hierarchy, Nanite, ray tracing, static lighting, or per-instance UObject state.

## Rendering APIs

The runtime component owns a small `FPrimitiveSceneProxy`. The proxy reads LOD0's
`FStaticMeshLODResources`, reuses the mesh asset's vertex and index buffers, and submits one dynamic
`FMeshBatch` per LOD0 section. A custom `FLocalVertexFactory` subclass adds the traditional instance
attributes expected by `/Engine/Private/LocalVertexFactory.ush`. It deliberately selects
`USE_INSTANCING` and opts out of GPU Scene instance culling and primitive-ID streams.

The implementation uses `FVertexBuffer`, `FRHIBufferCreateDesc`, render commands,
`FDynamicPrimitiveUniformBuffer`, `FMeshElementCollector`, and the engine's neutral
`FInstancedStaticMeshVFLooseUniformShaderParameters` layout. Materials must support the
`MATUSAGE_InstancedStaticMeshes` permutation; incompatible materials use the default surface
material.

## Data flow

Callers own their canonical instance data in whatever layout and datatypes suit the simulation. The
generated `InstanceData` SoA remains available as an optional container, but the component does not
depend on it.

`set_instances()` accepts the complete live instance count and a chunk callback. The callback reads
the caller's source arrays and writes transforms through `FSandboxISMCInstanceChunkWriter`, so source
conversion, final 64-byte GPU packing, and conservative bounds accumulation happen in one pass with
no intermediate transform array. `Auto`, `Sequential`, and `Parallel` policies control whether the
fixed 1,024-instance chunks are processed by `ParallelFor`; `Auto` switches at 4,096 instances.
Callers that already have conservative component-local bounds can pass them to `set_instances()` to
skip per-instance bounds accumulation.

The component owns three persistent packed staging arrays through `ml::MultiBuffer`. Each call fills
the next available array and retains its capacity for later updates. The render thread copies the
completed array through one offset-zero RHI buffer lock into a persistent vertex buffer, then marks
the staging array reusable. If all three arrays are exceptionally still in flight, the game thread
records a staging-buffer wait and flushes outstanding rendering work before reusing one. The GPU
buffer grows to a power-of-two capacity and is not reallocated while the active count remains within
that capacity. A shared zero stream supplies the unused lightmap-bias attribute required by the
engine shader layout. Changing the mesh or submitting zero instances clears the snapshot. Component
bounds reduce the per-chunk conservative mesh-sphere bounds in chunk order.

## Current update cost

An update now:

1. converts, packs, and accumulates bounds for the complete snapshot, optionally in parallel;
2. reduces the chunk bounds; and
3. uploads one contiguous packed array.

At 40,000 instances the payload is approximately 2.44 MiB. `stat SandboxISMC` shows snapshot build,
submission, and render-thread upload CPU scopes. The lab actor also logs the most recent timings,
instance count, and payload bytes.

## Version risk

The highest-risk dependency is the private engine shader contract: attribute locations 8 through 12,
the `USE_INSTANCING` defines, and the instanced loose-uniform layout can change between Unreal
versions. `FStaticMeshLODResources`, vertex-buffer binding helpers, mesh-batch fields, and low-level RHI
buffer creation are also version-sensitive. The code avoids renderer-private C++ headers and avoids
the substantially larger ISMC/GPU Scene instance-data manager, but it still requires review on every
engine upgrade.

## Next experiments

The next rendering decision should be driven by the expanded benchmark: coarse spatial render chunks
would provide useful frustum and shadow culling without immediately adopting the substantially more
complex GPU Scene instance-culling path. Later upload experiments can compare asynchronous snapshot
preparation and alternative RHI upload strategies.

Preliminary verdict: the approach is viable enough at the architecture and build level to run the
rendering experiment. Continuing to collision is justified if the editor lab confirms correct rendering
and its measured full-rebuild cost is acceptable as a baseline. The collision phase should keep the
same dense indices and compare an invisible query-only Unreal ISMC against a custom query-only spatial
structure; no Chaos integration belongs in this phase.

## Demo level

Open `/SandboxISMC/Lab/FT_SandboxISMC`. The map contains a lab actor configured with the
engine cube mesh, a 200 x 200 grid (40,000 instances), lighting, and a camera aimed at the grid. The
instances regenerate when the actor is constructed so they are visible in the editor. In Simulate or
PIE, the first 1,024 source instances rotate and the actor submits a complete snapshot every tick;
the Output Log reports update timings and payload bytes.

## ISMC comparison benchmark

Open `/SandboxISMC/Lab/FT_SandboxISMCBenchmark` and start PIE. The fixed camera shows two matching
40,000-instance grids: custom on the left and engine ISMC on the right. Every tick, the configured
contiguous percentage of both grids receives the same slow vertical translation and rotation. The
custom component rebuilds its complete snapshot while the engine ISMC uses its matching batch update.
The custom update and engine update run consecutively inside the same Game Thread frame, so their sibling
scopes can be compared directly. Select paired, custom-only, or engine-ISMC-only mode. The isolated
modes make whole-frame and GPU comparisons attributable to one renderer. There are no phases, warmups,
or automatic stop; stop PIE after capturing the interval you want.

The same benchmark can run unattended with a real offscreen rendering RHI:

```text
cmake --workflow --preset sandbox-ismc-benchmark
```

The remote workflow builds a Development editor, starts PIE for ten seconds with 40,000 instances per
renderer, flushes final render work, saves the CSV and Insights trace, ends PIE, and exits. Configure a
different duration or instance count before running its build and test presets:

```text
cmake --preset sandbox-ismc-benchmark -DSANDBOX_ISMC_BENCHMARK_SECONDS=30 -DSANDBOX_ISMC_BENCHMARK_INSTANCES=100000 -DSANDBOX_ISMC_BENCHMARK_UPDATE_PERCENT=10 -DSANDBOX_ISMC_BENCHMARK_MODE=custom -DSANDBOX_ISMC_BENCHMARK_VISIBILITY=half -DSANDBOX_ISMC_BENCHMARK_SHADOWS=1
cmake --build --preset sandbox-ismc-benchmark
ctest --preset sandbox-ismc-benchmark
```

Valid modes are `paired`, `custom`, and `engine_ismc`. Visibility is `all`, `half`, or `none`;
non-visible instances are placed behind the fixed camera while remaining in the component database.
Use paired mode for same-frame CPU API comparison, then matching custom and engine-only runs for GPU
comparison. Update percentage accepts 0 through 100. Shadows are disabled by default.
The benchmark actor's `Use Supplied Bounds` property switches the custom component between calculating
bounds during each snapshot and reusing conservative bounds calculated once during setup.

Do not add `-nullrhi`; the Render Thread and GPU comparison require a real RHI.

The comparison uses the same mesh, default material, local transforms, mobility, movement, and view.
Collision, overlap events, navigation contribution, character stepping, ray tracing visibility, and
distance culling are disabled on the engine ISMC and on the custom component where applicable. The
actor temporarily sets `r.VSync`, `r.VSyncEditor`, and `t.MaxFPS` to zero and restores their previous
values when PIE ends.

Results are written to `Saved/Benchmarks/SandboxISMC_*.csv` when PIE stops. The filename identifies
mode, update percentage, and visibility. Each metric reports its unit plus minimum, median, p95, and
maximum values over the complete run:

- `frame` is the PIE game-frame duration.
- `game_thread`, `render_thread`, and `gpu` are Unreal's whole-frame thread/GPU timers. They are most
  useful in isolated custom-only and engine-only runs; GPU startup samples reported as zero are
  omitted from the CSV.
- `total_update` covers the public component update path.
- `build` is the custom component's complete conversion, packing, and bounds pass.
- `prepare` is benchmark-side construction of the changed engine ISMC transforms.
- `api` is `set_instances()` or `BatchUpdateInstancesTransforms()`.
- `uploaded` is the custom snapshot payload size.

When tracing is enabled, the actor records `Saved/Profiling/SandboxISMC_Paired_*.utrace`, including
CPU, GPU, frame, bookmark, counter, stats, render-command, and RHI-command channels. An editor `Failed
to connect to the store client` message does not invalidate a file trace when the log also reports
`Trace started (writing to file ...)`.

### Reading the Insights trace

On the Game Thread, expand `ASandboxISMCBenchmarkActor::Tick`. In paired mode every frame contains
these two sibling scopes:

- `ASandboxISMCBenchmarkActor::update_custom`, containing the custom snapshot build.
- `ASandboxISMCBenchmarkActor::update_engine_ismc`, containing transform preparation and
  `BatchUpdateInstancesTransforms()`.

The nested scopes separate caller-side preparation from component API cost. Creation is captured as
`ASandboxISMCBenchmarkActor::create_instances`; the custom and engine creation counters provide its
per-renderer split.

Filter timers by `SandboxISMC` to follow the custom path across threads:

- `USandboxISMCComponent::set_instances` contains the complete conversion, packing, and
  bounds pass.
- `USandboxISMCComponent::SendRenderDynamicData_Concurrent` submits the snapshot.
- `FSandboxISMCInstanceBuffer::upload` uploads the packed buffer on the Render Thread.

The Render Thread also writes `SandboxISMC/RenderThreadUploadMs`,
`SandboxISMC/RenderThreadUploadBytes`, and `SandboxISMC/RenderThreadUploadInstances` directly after
every upload. Use these counters when inspecting upload cost because they are written on the Render
Thread without asynchronous Game Thread readback. The custom component commits every tick, so these
counters receive a new sample every frame.

Add counters beginning with `SandboxISMCBenchmark/Custom/` and
`SandboxISMCBenchmark/EngineISMC/` to the same graph to overlay total update, build/preparation, and API
time. `SandboxISMCBenchmark/FrameMs` provides frame context. Engine ISMC's normal GPU Scene and
culling path remains enabled because it is part of the renderer being compared; the chosen cube has
one LOD, matching the custom renderer's LOD0-only limitation.
