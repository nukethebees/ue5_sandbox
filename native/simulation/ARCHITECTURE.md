# Fixed tick

| Phase | Data available |
| --- | --- |
| Preparation | Commit spawns. Publish lookup rows and spatial index. |
| Thinking | Read the snapshot. Choose tasks, movement, and fire commands. |
| Action | Move and fire. Refresh spatial membership. |
| Resolution | Apply damage and publish deaths. Sample level logic while rows still exist. |
| ResolutionCommit | Remove entities and compact storage. Row handles expire. |
| BetweenTicks | Read fresh presentation views and dispatch tick-end events. |

Preparation drains carrier launches in FIFO order up to each team's cap. Authored fighters bypass the cap. `SpatialQueryManager` owns the spatial index.

A `LevelEntityRef` names an authored slot. Its `EntityId` and ledger history survive death. Reacquire views after storage moves. [Phase implementation](src/levels/level_sim.cpp) · [Level timing](../../LevelScripts/docs/logic.md)
