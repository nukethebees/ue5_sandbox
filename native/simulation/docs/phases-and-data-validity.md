# Phases and data validity

Each tick advances in one direction:

`BetweenTicks -> Preparation -> Thinking -> Action -> Resolution -> ResolutionCommit -> BetweenTicks`

`SimClock::transition_to` asserts the permitted transitions. Pausing is a separate orchestrator
state; `BetweenTicks` performs no work and introduces no wait.

Initialisation spawns and configures entities. Each type publishes its initial lookup state before
entering `StableSetup`. Mission initialisation and setup queries run there, without structural
mutation, before entering `BetweenTicks`.

| Phase | Storage layout and lookup rows | Component values |
|---|---|---|
| Preparation | Commit spawns/orders and reorder; each type publishes when ready | Prepare next snapshot |
| Thinking | Stable | Preserve published spatial, team, and health query state |
| Action | Stable | Apply movement/rotation and materialise actions |
| Resolution | Stable until commit | Apply damage, resolve secondary effects, publish deaths |
| ResolutionCommit | Remove and reorder; previous handles/views expire | Compact aligned storage |
| BetweenTicks | Previous published handles must not be used | Acquire fresh typed presentation views |

General entity lookups are permitted only in StableSetup, Thinking, Action, and Resolution.
Preparation may still hold stale rows while owners spawn or reorder. Each owner updates its own
table when ready; dependent cross-type reads begin in Thinking, after all updates finish.
Each update rebuilds complete handles for all active rows. Health changes are not queued.

## Distinguish the validity guarantees

- **Stable identity:** `EntityUniqueId` lasts for the game, including after death. An identity does
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

Refresh spatial-grid membership after removals so geometry-only traces remain available between
ticks. Range queries that resolve entity columns require a published lookup phase; a live grid
does not extend the lifetime of the previous lookup rows.

The consolidated phase-invariant check verifies Thinking's published rows, health alignment,
metadata, and live spatial-grid membership. Structural and phase-transition assertions enforce
the boundaries rather than repeating those checks for every query candidate.
