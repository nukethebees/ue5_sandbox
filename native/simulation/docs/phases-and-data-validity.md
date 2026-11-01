# Phases and data validity

Each tick advances in one direction:

`BetweenTicks -> Preparation -> Thinking -> Action -> Resolution -> ResolutionCommit -> BetweenTicks`

`SimClock::transition_to` asserts the permitted transitions. `LevelState` owns the level lifecycle;
`BetweenTicks` performs no work and introduces no wait.

Each catch-up tick samples player simulation presence and health, then dispatches a
`TickPlayerShipState` specialization for all of that tick's phases: `NotPresent`, `PresentAlive`,
or `PresentDead`. A death during Resolution affects the next tick's dispatch.

Initialisation spawns and configures entities. Each type publishes its initial lookup state before
entering `LevelState::StableSetup`. Mission initialisation and setup queries run there, without
structural mutation, before entering `LevelState::Paused`. Setup uses Preparation for population
and Thinking for initial queries, then resets the clock to `BetweenTicks`, tick zero, and an empty
time accumulator. Configured tick rate and time scale are preserved.

| Phase | Storage layout and lookup rows | Component values |
|---|---|---|
| Preparation | Commit spawns/orders and reorder; each type publishes when ready | Prepare next snapshot |
| Thinking | Stable | Preserve published spatial, team, and health query state |
| Action | Stable | Apply movement/rotation and materialise actions |
| Resolution | Stable until commit | Apply damage, resolve secondary effects, publish deaths |
| ResolutionCommit | Remove and reorder; previous handles/views expire | Compact aligned storage |
| BetweenTicks | Previous published handles must not be used | Acquire fresh typed presentation views |

General entity lookups are permitted in `LevelState::StableSetup`, or while Running in Thinking,
Action, and Resolution. Initialisation permits structural mutation but forbids general lookups;
Paused and Stopped permit neither. The clock owns no lifecycle state: its predicates check the
current operation's phase. Callers must establish the lifecycle contract at level boundaries;
`is_lookup_allowed(phase, state)` is available when both values are needed explicitly.
Preparation may still hold stale rows while owners spawn or reorder. Each owner updates its own
table when ready; dependent cross-type reads begin in Thinking, after all updates finish.
Each update rebuilds complete handles for all active rows. Health changes are not queued.

## Distinguish the validity guarantees

- **Stable identity:** `EntityId` lasts for the game, including after death. An identity does
  not imply a currently live entity.
- **Row correspondence:** an active handle's row identifies the same entity from publication through
  pre-compaction Resolution. Changing position or health does not itself invalidate that row.
- **Packed metadata:** team and quantised health form a phase-scoped snapshot, guaranteed current
  during Thinking. Do not use packed health as authoritative health during Resolution.
- **Borrowed views:** do not retain views across reallocation, buffer cycling, reordering, or removal.
  Acquire current typed views for presentation after compaction.
- **Scratch results:** a stable row does not extend allocation lifetime. A scratch-backed container
  expires when its owning scratch scope ends or its frame resource resets. Caller ownership does
  not permit it to outlive that resource.

`LevelMemoryManager` owns separate tick and phase `ioj::FrameMemoryResource` instances, each with
a configurable 256 MiB default backing allocation obtained through `GameMemory`. Persistent level
allocations also use `GameMemory`. Both scratch resources forward requests that do not fit their
backing upstream and return forwarded allocations upstream when their containers deallocate them.
`GameMemory` serializes upstream access and allocation accounting across both resources.

Simulation operations currently receive phase memory. `ioj::FrameScratchScope` covers each phase
that uses scratch and reclaims its backing on exit. Destroy its containers and complete parallel
work before leaving the phase. Tick memory supports data passed between phases and shorter-lived
temporaries; destroy those containers before the tick ends. Both resources finalize their per-tick
statistics at the end of each tick, including every catch-up tick within `advance()`.

Reclamation requires all local and forwarded allocations from that resource to have been released.
No allocation or borrowed view may cross its resource's reclamation boundary. Local claimed-byte
statistics describe backing usage; payload and allocation counts also include forwarded requests.
The benchmark's frame metrics aggregate both resources, summing their separate backing peaks.

Health bands are `(0,25%]`, `(25,50%]`, `(50,75%]`, and `(75,100%]` of the existing maximum health.
Overheal uses the highest band. Death is not a fifth band: no dead entity is published as active
for Thinking. During Resolution, use authoritative health to distinguish a retained dead row.

## End-of-tick ordering

1. Resolve all damage and secondary death effects.
2. Publish deaths to the persistent ledger.
3. Gather authoritative objective health and evaluate the mission.
4. Enter ResolutionCommit and compact storage.
5. Finalise frame outputs, telemetry, and tick accounting; enter BetweenTicks.

Mission evaluation sees same-tick deaths before compaction. It does not depend on the packed health
snapshot. Refresh surviving lookup rows in the next Preparation; do not rebuild the lookup tables
for presentation after compaction.

The entity grid shares the lookup validity window. Rebuild it after Preparation and after Action
movement, before resolving damage. Do not query it after structural removals or between ticks;
the next Preparation publishes current rows and grid membership together. Debug presentation may
read the grid's owned bounds as a snapshot of the last rebuild, not as current entity membership.

The consolidated phase-invariant check verifies Thinking's published rows, health alignment,
metadata, and live spatial-grid membership. Structural and phase-transition assertions enforce
the boundaries rather than repeating those checks for every query candidate.
