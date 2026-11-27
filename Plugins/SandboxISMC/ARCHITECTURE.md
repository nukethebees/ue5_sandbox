# SandboxISMC

SpaceGame renders unit-scale fighters and lasers. Supply resident, inlined LOD0 buffers. Replacing the mesh invalidates the snapshot.

Each 16-byte transform stores three finite component-space floats and a 32-bit packed rotation. The level collision grid sets gameplay bounds. [Native layout](../../native/ismc/lib/include/ioj/ismc/transform.h) · [Shader input](Shaders/Private/PackedInstanceInput.ush)

The SandboxISMC vertex factory uses the custom instance-input hook in the [project's UE 5.8.4 patch series](../../unreal/engine/README.md).

[Run the instance lab](../SandboxISMCLab/README.md)
