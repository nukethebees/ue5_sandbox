# SandboxISMC architecture

SandboxISMC renders a dense snapshot of unit-scale instances of one mesh. SpaceGame submits
fighter and laser positions, normalized rotations and optional custom material data. The component
packs the snapshot on the CPU and the vertex factory decodes it on the GPU.

## Meshes and bounds

The mesh needs resident, inlined, non-optional LOD0 vertex and index buffers in the cooked asset.
Mesh replacement invalidates the previous snapshot. Each non-empty submission supplies a finite
position domain containing every instance. Render bounds can be computed from the mesh or supplied
by the caller. Include material vertex displacement in supplied bounds. The component adds the
margin for transform quantisation.

## Snapshot lifetime and format

Three reusable CPU staging slots separate packing from render-thread uploads. A queued upload
retains its slot until the render thread copies it. GPU buffers retain capacity and upload the
live prefix. The snapshot carries its own position origin so an asynchronous upload decodes
against the origin used for packing.

The little-endian transform occupies 12 bytes with four-byte alignment. Bytes 0–5 hold three
signed 16-bit position offsets in steps of 16 Unreal units. Bytes 6–7 are reserved. Bytes 8–11 hold
a smallest-three encoded quaternion. The native packer and shader decoder share this format.
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
