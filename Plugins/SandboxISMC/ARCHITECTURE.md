# SandboxISMC

SpaceGame renders unit-scale fighters and lasers. Supply resident, inlined LOD0 buffers. Replacing the mesh invalidates the snapshot.

Each 16-byte transform stores three finite component-space floats and a 32-bit packed rotation. The mission collision grid sets gameplay bounds. [Native layout](../../native/ismc/lib/include/ioj/ismc/transform.h) · [Shader input](Shaders/Private/PackedInstanceInput.ush)

Apply the local vertex factory hook once per engine checkout. After an engine upgrade, check it from the repository root:

```powershell
python Plugins/SandboxISMC/Tools/apply_local_vertex_factory_hook.py "$env:UE_ROOT"
python Plugins/SandboxISMC/Tools/apply_local_vertex_factory_hook.py "$env:UE_ROOT" --check
```

[Run the instance lab](../SandboxISMCLab/README.md)
