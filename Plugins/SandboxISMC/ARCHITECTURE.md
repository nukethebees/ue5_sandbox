# SandboxISMC

SpaceGame submits unit-scale fighter and laser instances to the renderer. Supply resident, inlined LOD0 buffers and a finite position domain enclosing each submitted instance. Replacing the mesh invalidates the snapshot. The [native packer](../../native/ismc/lib/include/ioj/ismc/transform.h) and [shader decoder](Shaders/Private/PackedTransform.ush) share a 12-byte transform.

Apply the local vertex factory hook once per engine checkout. After an engine upgrade, check it from the repository root:

```powershell
python Plugins/SandboxISMC/Tools/apply_local_vertex_factory_hook.py "$env:UE_ROOT"
python Plugins/SandboxISMC/Tools/apply_local_vertex_factory_hook.py "$env:UE_ROOT" --check
```

[Run the instance lab](../SandboxISMCLab/README.md)
