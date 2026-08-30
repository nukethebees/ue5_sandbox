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

`commit_instance_updates()` converts dirty SoA ranges into immutable render update packets. Each GPU
instance contains an origin and three scaled-rotation rows, for 64 bytes per instance. Packets cross
to the render thread, where their byte ranges are copied through RHI-managed staging memory into one
persistent vertex buffer. The buffer grows to a power-of-two capacity and is not reallocated while
the active count remains within that capacity. A shared zero stream supplies the unused lightmap-bias
attribute required by the engine shader layout.

`set_instance_transform()` marks its index automatically. Mutable whole-array accessors conservatively
mark every instance dirty. Callers doing bulk partial work should use `edit_positions()`,
`edit_rotations()`, and `edit_scales()` with the same range; adjacent and overlapping ranges are
coalesced before packing. Additions dirty the appended instance. Creation, buffer growth,
swap-removal/reordering, mesh changes, and multiple unsent commits fall back to a full snapshot.

Component bounds use a conservative transformed mesh sphere per instance. Sparse updates maintain a
binary bounds tree from only the changed leaves and their ancestors. A full-range update uses a
faster linear aggregate and invalidates the tree; the first subsequent sparse update rebuilds it once.
Creation, removal, mesh changes, and bounds-capacity growth also rebuild the tree. The sphere bounds
can be looser for elongated meshes, but remain safe for culling.

## Current update cost

An ordinary partial commit now:

1. packs only coalesced dirty ranges;
2. updates the corresponding bounds-tree leaves and ancestors; and
3. uploads only those packed byte ranges.

At 40,000 instances a full payload is approximately 2.44 MiB; updating 10% as one contiguous range is
approximately 250 KiB. `stat SandboxISMC` shows preparation, submission, and render-thread upload CPU
scopes. The lab actor also logs the most recent timings, instance count, dirty count/ranges, and
payload bytes.

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
complex GPU Scene instance-culling path. Later upload experiments can compare a packed CPU mirror,
double buffering, and asynchronous snapshot preparation.

Preliminary verdict: the approach is viable enough at the architecture and build level to run the
rendering experiment. Continuing to collision is justified if the editor lab confirms correct rendering
and its measured full-rebuild cost is acceptable as a baseline. The collision phase should keep the
same dense indices and compare an invisible query-only Unreal ISMC against a custom query-only spatial
structure; no Chaos integration belongs in this phase.

## Demo level

Open `/SandboxISMC/Lab/FT_SandboxISMC`. The map contains a lab actor configured with the
engine cube mesh, a 200 x 200 grid (40,000 instances), lighting, and a camera aimed at the grid. The
instances regenerate when the actor is constructed so they are visible in the editor. In Simulate or
PIE, the first 1,024 instances rotate and commit one partial range every tick; the Output Log reports
update timings and payload bytes.

## ISMC comparison benchmark

Open `/SandboxISMC/Lab/FT_SandboxISMCBenchmark` and start PIE. The fixed camera shows two matching
40,000-instance grids: custom on the left and engine ISMC on the right. Every tick, the configured
contiguous percentage of both grids receives the same slow vertical translation and rotation. The
custom update and engine update run consecutively inside the same Game Thread frame, so their sibling
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
- `total_update` covers preparation plus the public component update call.
- `prepare` is benchmark-side writing or construction of the changed transforms.
- `api` is `commit_instance_updates()` or `BatchUpdateInstancesTransforms()`.
- `pack` and `bounds` split the custom component's dirty-range packing and bounds-tree update;
  both are contained within `api`, not additional costs.
- `uploaded`, `dirty_instances`, and `dirty_ranges` expose the custom commit packet size and shape.

When tracing is enabled, the actor records `Saved/Profiling/SandboxISMC_Paired_*.utrace`, including
CPU, GPU, frame, bookmark, counter, stats, render-command, and RHI-command channels. An editor `Failed
to connect to the store client` message does not invalidate a file trace when the log also reports
`Trace started (writing to file ...)`.

### Reading the Insights trace

On the Game Thread, expand `SandboxISMCBenchmark_FrameUpdates`. In paired mode every frame contains
these two sibling scopes:

- `SandboxISMCBenchmark_CustomUpdate`, containing transform preparation and the custom commit.
- `SandboxISMCBenchmark_EngineISMCUpdate`, containing transform preparation and
  `BatchUpdateInstancesTransforms()`.

The nested scopes separate caller-side preparation from component API cost. Creation is captured as
`SandboxISMCBenchmark_CustomCreateInstances` and
`SandboxISMCBenchmark_EngineISMCCreateInstances`.

Filter timers by `SandboxISMC_Custom` to follow the custom path across threads:

- `SandboxISMC_Custom_PackAndBounds` contains the complete custom preparation pass.
- `SandboxISMC_Custom_PackTransforms` packs the SoA transforms.
- `SandboxISMC_Custom_UpdateConservativeBounds` updates the conservative bounds tree.
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
