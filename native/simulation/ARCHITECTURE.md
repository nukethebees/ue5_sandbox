# Simulation architecture

`LevelSim` owns the world and advances it in fixed ticks. Each entity simulation owns its
component data. The entity ledger keeps lifetime history, and the collision pipeline owns the
spatial index. The task manager assigns work to fighters. The phase order is defined in
[level_sim.cpp](src/levels/level_sim.cpp).

| Phase | Work and data validity |
| --- | --- |
| Preparation | Commit queued spawns and publish lookup rows and the spatial index. Cross-entity reads can begin when publication finishes. |
| Thinking | Choose tasks, movement and fire commands using the published snapshot. |
| Action | Apply movement and firing. Refresh spatial membership for overlap detection. |
| Resolution | Apply damage and publish deaths. Mission logic can observe a death on this tick while its row still exists. |
| ResolutionCommit | Remove entities and compact storage. Published row handles expire. |
| BetweenTicks | Read fresh presentation views. Tick-end events can pause or stop the level. |

## Identity and lifetime

A `LevelEntityRef` names an authored slot. The spawn manager fills it with a lifetime `EntityId`
when the entity spawns. The ID and its ledger history survive death. Lookup tables map a live ID
to its current row. Authoritative health is indexed by lifetime ID within each entity type.
The quantised health in a lookup handle is a Thinking snapshot.

Reacquire borrowed views after storage is reordered or resized. Use phase scratch for work
within one phase and tick scratch for data passed to a later phase of the same tick.

## Mission logic

During Resolution, `LevelSim` commits the previous tick's latch captures, advances timers,
evaluates conditions and collects ready triggers. A latch captures once and becomes visible on
the next tick. A trigger fires once. Spawn events run in Preparation. Timer starts, pause and
terminal events run at tick end, in queue order. The first terminal event decides the result.

See the [mission authoring guide](../../LevelScripts/docs/logic.md) and the
[Unreal runtime guide](../../Plugins/SpaceGame/ARCHITECTURE.md).
