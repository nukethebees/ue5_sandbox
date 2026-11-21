# Build ownership

```mermaid
flowchart TD
    P[Preset generator] --> C[CMake]
    C --> N[Native libraries and tools]
    C --> G[Generated inputs]
    N --> U[UnrealBuildTool]
    G --> U
    U --> E[Editor and game]
```

Regenerate presets with `coj presets` and verify with `coj presets --check`. Edit Unreal modules in their `.Build.cs` files and targets in `.Target.cs` files. [Build and test](../docs/build-and-test.md) · [CMake style](STYLE.md)
