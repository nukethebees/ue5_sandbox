# SpaceGame runtime

```mermaid
flowchart TD
    M[Saved mission] --> O[ATestBatchOrchestrator]
    O -->|build native world| S[LevelSim]
    O -->|bind actor resources| H[FLevelPresentationHost]
    S -->|publish after fixed ticks| H
    H --> V[Player view and HUD]
```

Loading creates the player or observer, harvests static collision, and constructs a paused simulation. Automatic launches start it when requested. Presentation reads the final tick of each Unreal frame. Editor proxies supply placements through export.

Configure impact sparks and preview fighter spawn slots in the level's presentation and collision tools. [Author a level](../../LevelScripts/README.md) · [Simulation phases](../../native/simulation/ARCHITECTURE.md) · [Property categories](Categories.md)
