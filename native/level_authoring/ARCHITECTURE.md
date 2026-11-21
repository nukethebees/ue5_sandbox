# Level loading

```mermaid
flowchart TD
    S[Scheme mission or campaign] -->|evaluate imports| R[sandbox-s7]
    R -->|owned AST| A[native-level-authoring]
    A -->|decode records| V[native-levels validation]
    V --> N[Native simulation]
    V --> U[Unreal level loader]
```

Catalog entries retain their own diagnostics, so a broken mission leaves its siblings loadable. [Author a mission](../../LevelScripts/README.md) · [Script-facing records](../../lispb/schema/level_inputs.lispb)
