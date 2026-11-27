# Level loading

```mermaid
flowchart TD
    S[Scheme level or campaign] -->|evaluate imports| R[sandbox-s7]
    R -->|owned AST| A[native-level-authoring]
    A -->|decode records| V[native-levels validation]
    V --> N[Native simulation]
    V --> U[Unreal level loader]
```

The catalog and each entry share one interpreter. Each gets its own nested-import budget and diagnostics, so a broken level leaves its siblings loadable. [Author a level](../../LevelScripts/README.md) · [Script-facing records](../../lispb/schema/level_inputs.lispb)
