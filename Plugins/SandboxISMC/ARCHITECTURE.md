# SandboxISMC architecture

SandboxISMC renders a dense snapshot of unit-scale instances of one mesh. SpaceGame submits
fighter and laser positions, normalized rotations and optional custom material data. The component
packs the snapshot on the CPU and the vertex factory decodes it on the GPU.

## Meshes and bounds

The mesh needs resident, inlined, non-optional LOD0 vertex and index buffers in the cooked asset.
Mesh replacement invalidates the previous snapshot. Render bounds can be computed from the mesh
or supplied by the caller. Include material vertex displacement in supplied bounds. The component
adds a margin for packed rotation and floating-point arithmetic.

## Snapshot lifetime and format

Three reusable CPU staging slots separate packing from render-thread uploads. A queued upload
retains its slot until the render thread copies it. GPU buffers retain capacity and upload the
live prefix.

Each 16-byte transform stores finite component-space XYZ as three floats, then a packed quaternion
at byte 12. At ±2,500 km from the component origin, adjacent coordinates are 16 cm apart.
Gameplay boundaries come from the mission collision grid.
[Schema](../../lispb/schema/sandbox_ismc.lispb), [native transform contract](../../native/ismc/lib/include/ioj/ismc/transform.h)
and [shader decoder](Shaders/Private/PackedTransform.ush).

## Engine integration

The plugin uses an engine shader hook to supply packed instance inputs to Unreal's local vertex
factory. Apply the hook once per engine checkout and check it after engine upgrades. Run these
commands from the repository root in PowerShell.

```powershell
python Plugins/SandboxISMC/Tools/apply_local_vertex_factory_hook.py "$env:UE_ROOT"
python Plugins/SandboxISMC/Tools/apply_local_vertex_factory_hook.py "$env:UE_ROOT" --check
```

See the [lab guide](../SandboxISMCLab/README.md) for the comparison workload and
[benchmark guide](../../tools/rust/crates/benchmark-tools/docs/unreal-benchmarks.md) for capture
commands. Upload timing measures render-thread CPU work. GPU and frame timings cover the scene.
