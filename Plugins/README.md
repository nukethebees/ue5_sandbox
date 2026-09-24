# Plugin map

`Plugins/` contains reusable engine extensions, experiments, shared game layers, and game plugins.
Use this map to find the owner before following a plugin's own README or source layout.

| Group | Plugins |
| --- | --- |
| Core and shared game systems | `SandboxCore`, `SandboxGameShared`, `SandboxISMC`, `SandboxMesh`, `SandboxNiagara` |
| Rendering, materials, and UI | `SandboxShaders`, `SandboxMaterialExprs`, `SandboxUI`, `USFLoader` |
| Game and authoring | `SpaceGame`, `SandboxEditorTools`, `ShooterGame`, `SGLegacy` |
| Experiments and tutorials | `SandboxGpuTutorials` |
| Third-party/editor integration | `VisualStudioTools` |

Several plugins already have local documentation: [SpaceGame](SpaceGame/README.md),
[SandboxCore storage](SandboxCore/Source/SandboxCore/single_allocation_storage.md),
[SandboxShaders](SandboxShaders/README.md), [SandboxUI](SandboxUI/README.md),
[SandboxISMC](SandboxISMC/README.md), and [SandboxGpuTutorials](SandboxGpuTutorials/README.md).

Plugin descriptors and module rules remain the authoritative record of what is enabled. Regenerate
project files after changing a plugin, module, `.Build.cs`, or `.Target.cs` definition.
