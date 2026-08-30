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
