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

The canonical CPU database is three component-local arrays: `FVector3f` positions, `FQuat4f`
rotations, and `FVector3f` scales. Dense integer indices provide direct access. Removal is an O(1)
swap-removal, and the result identifies the previous last index when another data set needs to mirror
that move.

`commit_instance_updates()` converts the SoA arrays into an immutable render snapshot. Each GPU
instance contains an origin and three scaled-rotation rows, for 64 bytes per instance. The snapshot
crosses to the render thread, where it is copied into one dynamic vertex buffer. A shared zero stream
supplies the unused lightmap-bias attribute required by the engine shader layout.

Component bounds use a conservative transformed mesh sphere per instance. This avoids transforming
all eight mesh-box corners in double precision for every commit. The resulting bounds can be looser
for elongated meshes, but remain safe for culling and are appropriate for this experiment.

## Current update cost

Every commit currently:

1. visits every instance to build the packed snapshot;
2. visits every instance while rebuilding component bounds; and
3. uploads every active packed instance, even when only one transform changed.

At 40,000 instances the active payload is approximately 2.44 MiB per commit. `stat SandboxISMC`
shows preparation, submission, and render-thread upload CPU scopes. The lab actor also logs the most
recent timings, instance count, and payload bytes.

## Version risk

The highest-risk dependency is the private engine shader contract: attribute locations 8 through 12,
the `USE_INSTANCING` defines, and the instanced loose-uniform layout can change between Unreal
versions. `FStaticMeshLODResources`, vertex-buffer binding helpers, mesh-batch fields, and low-level RHI
buffer creation are also version-sensitive. The code avoids renderer-private C++ headers and avoids
the substantially larger ISMC/GPU Scene instance-data manager, but it still requires review on every
engine upgrade.

## Next experiments

If the Phase 1 lab renders and updates the target instance counts reliably, the next rendering
experiments are dirty-index collection, coalesced dirty ranges, partial buffer locks, retaining a packed
CPU mirror, double buffering, and asynchronous snapshot preparation. The present design keeps those
changes at the SoA-to-render boundary.

Preliminary verdict: the approach is viable enough at the architecture and build level to run the
rendering experiment. Continuing to collision is justified if the editor lab confirms correct rendering
and its measured full-rebuild cost is acceptable as a baseline. The collision phase should keep the
same dense indices and compare an invisible query-only Unreal ISMC against a custom query-only spatial
structure; no Chaos integration belongs in this phase.

## Demo level

Open `/SandboxISMC/Lab/FT_SandboxISMC`. The map contains a lab actor configured with the
engine cube mesh, a 200 x 200 grid (40,000 instances), lighting, and a camera aimed at the grid. The
instances regenerate when the actor is constructed so they are visible in the editor. In Simulate or
PIE, the first 1,024 instances rotate and commit a full update every tick; the Output Log reports update
timings and the approximately 2.44 MiB upload payload.

## ISMC comparison benchmark

Open `/SandboxISMC/Lab/FT_SandboxISMCBenchmark` and start PIE. The fixed camera shows two matching
40,000-instance grids: custom on the left and engine ISMC on the right. Every tick, both grids receive
the same slow vertical translation and rotation. The custom update and engine update run consecutively
inside the same Game Thread frame, so their sibling scopes can be compared directly. There are no
phases, warmups, or automatic stop; stop PIE after capturing the interval you want.

The same benchmark can run unattended with a real offscreen rendering RHI:

```text
cmake --workflow --preset sandbox-ismc-benchmark
```

The remote workflow builds a Development editor, starts PIE for ten seconds with 40,000 instances per
renderer, flushes final render work, saves the CSV and Insights trace, ends PIE, and exits. Configure a
different duration or instance count before running its build and test presets:

```text
cmake --preset sandbox-ismc-benchmark -DSANDBOX_ISMC_BENCHMARK_SECONDS=30 -DSANDBOX_ISMC_BENCHMARK_INSTANCES=100000
cmake --build --preset sandbox-ismc-benchmark
ctest --preset sandbox-ismc-benchmark
```

Do not add `-nullrhi`; the Render Thread and GPU comparison require a real RHI.

The comparison uses the same mesh, default material, local transforms, mobility, movement, and view.
Collision, overlap events, navigation contribution, character stepping, ray tracing visibility, and
distance culling are disabled on the engine ISMC and on the custom component where applicable. The
actor temporarily sets `r.VSync`, `r.VSyncEditor`, and `t.MaxFPS` to zero and restores their previous
values when PIE ends.

Results are written to `Saved/Benchmarks/SandboxISMC_Paired_*.csv` when PIE stops. Each metric reports
minimum, median, p95, and maximum values over the complete run:

- `frame` is the PIE game-frame duration.
- `total_update` covers preparation plus the public component update call.
- `prepare` is benchmark-side writing or construction of the changed transforms.
- `api` is `commit_instance_updates()` or `BatchUpdateInstancesTransforms()`.
- `pack` and `bounds` split the custom component's snapshot packing and conservative bounds rebuild;
  both are contained within `api`, not additional costs.

When tracing is enabled, the actor records `Saved/Profiling/SandboxISMC_Paired_*.utrace`, including
CPU, GPU, frame, bookmark, counter, stats, render-command, and RHI-command channels. An editor `Failed
to connect to the store client` message does not invalidate a file trace when the log also reports
`Trace started (writing to file ...)`.

### Reading the Insights trace

On the Game Thread, expand `SandboxISMCBenchmark_PairedFrameUpdates`. Every frame contains these two
sibling scopes:

- `SandboxISMCBenchmark_CustomUpdate`, containing transform preparation and the custom commit.
- `SandboxISMCBenchmark_EngineISMCUpdate`, containing transform preparation and
  `BatchUpdateInstancesTransforms()`.

The nested scopes separate caller-side preparation from component API cost. Creation is captured as
`SandboxISMCBenchmark_CustomCreateInstances` and
`SandboxISMCBenchmark_EngineISMCCreateInstances`.

Filter timers by `SandboxISMC_Custom` to follow the custom path across threads:

- `SandboxISMC_Custom_PackAndBounds` contains the complete custom preparation pass.
- `SandboxISMC_Custom_PackTransforms` packs the SoA transforms.
- `SandboxISMC_Custom_RebuildConservativeBounds` rebuilds conservative component bounds.
- `SandboxISMC_Custom_SubmitRenderUpdate` submits the snapshot.
- `SandboxISMC_Custom_RenderThreadUpload` uploads the packed buffer on the Render Thread.

The Render Thread also writes `SandboxISMC/RenderThreadUploadMs`,
`SandboxISMC/RenderThreadUploadBytes`, and `SandboxISMC/RenderThreadUploadInstances` directly after
every upload. Use these counters when inspecting upload cost because they are written on the Render
Thread without asynchronous Game Thread readback. The custom component commits every tick, so these
counters receive a new sample every frame.

Add counters beginning with `SandboxISMCBenchmark/Custom/` and
`SandboxISMCBenchmark/EngineISMC/` to the same graph to overlay total update, preparation, and API
time. `SandboxISMCBenchmark/FrameMs` provides frame context. Engine ISMC's normal GPU Scene and
culling path remains enabled because it is part of the renderer being compared; the chosen cube has
one LOD, matching the custom renderer's LOD0-only limitation.
