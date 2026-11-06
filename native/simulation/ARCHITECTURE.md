# Simulation architecture

The simulation uses explicit, compile-time systems and generated SoA containers. Entity identity,
active storage layout, and component lifetime are separate concerns.

See [phases and data validity](docs/phases-and-data-validity.md) for the publication and mutation
contract.

## Level ownership and phases

`LevelSim` owns the clock, entity ledger and tables, concrete entity simulations, collision,
mission, authored events and telemetry. It remains nonmovable because those owners retain
references to one another. Declaration order keeps the clock, tables and frame-memory backing
alive until their consumers have been destroyed.

The level coordinates Preparation, Thinking, Action and Resolution through small, explicit
phase methods. Each entity simulation receives only its phase inputs: queries, command sinks,
combat events or scratch storage as required. These inputs borrow existing owners for the call;
they do not change SoA storage or add runtime system dispatch.

`collision::CollisionPipeline` owns the spatial-query manager, overlap response and reusable
candidate IDs. It copies newly spawned identities after spawn commits and before entity-row
preparation can reorder storage. Movement adds player and fighter candidates; the pipeline
deduplicates them and refreshes the spatial index before overlap detection. Queries and temporary
overlap results remain subject to the phase and scratch-lifetime contracts.

Construction spawns initial entities but leaves the level in `LevelState::Uninitialised`. The
host may install static geometry before calling `finish_initialisation()`, which publishes entity lookups
and the spatial index, begins telemetry and resets mission progress in `LevelState::StableSetup`, then enters
`LevelState::Paused`. Setup uses Preparation for population and Thinking for stable queries, then
resets the independent clock to tick zero and `BetweenTicks` before returning. Its phase enum
contains only the repeating tick cycle. Mission evaluation during a tick
follows published deaths and precedes `ResolutionCommit`, while entity row mappings are still valid.

Mission completions remain available until `reset_progress()`; queries do not consume results.
After advancing the simulation, the orchestrator publishes a completion only when the winning
record's tick is newer than the previous completed tick. `LevelSim` stops on mission completion
and finalizes telemetry with a neutral completion reason; Mission retains victory and defeat.
Duration limits compile to timer-triggered stop events. Living-team counts do not end a run.
Battle launches without an explicit duration currently use
a 24-hour simulation-time placeholder and can be ended manually.

`LevelEventManager` maps expression roots to ranges of packed event handles. Resolved triggers
retire after Resolution, queueing their events once when true. `LevelSim` sorts the queue by kind
at the completed-tick boundary: mission completion precedes duration expiry, and either discards
pending spawns and stops catch-up. Otherwise, Preparation of the following tick routes the spawn
span to `LevelSpawnManager`. Initial spawns and recurring carrier production remain separate
from these one-shot events.

Unreal lifecycle and presentation ownership are described in the
[SpaceGame architecture](../../Plugins/SpaceGame/ARCHITECTURE.md).

## Entity lookup

`EntityId` is the persistent external identity: entity type and type-local lifetime offset.
Death does not erase its history or make the identity available for reuse. Each entity type owns
a lookup table from that offset to a generated 32-bit `EntityInstanceHandle`:

- 18 bits for the active row;
- 3 bits for team;
- 2 bits for quantised health;
- 9 reserved bits, initially zero.

The 18-bit limit applies to simultaneous active rows, not lifetime identities. Preserve each
type's existing lifetime allocation budget independently of its active-row limit.

Resolve batches grouped by entity type. Bind each type's table and component spans once per run.
Keep temporary outputs caller-owned, with the caller choosing the scratch resource. Do not retain
active handles across structural mutation.

Health uses fixed type ranges and the active row directly. Its range base is known at compile time.
Each type rebuilds every active handle from the current row, team, and health during Preparation.
Health writes directly update the fixed ranges; no dirty flags or change queues are maintained.
Packed health is a Thinking snapshot. Existing consumers continue to use authoritative component
data until explicitly migrated.

## Roadmap, not scope of this refactor

The intended ECS/component model is compile-time and code-generated, not a runtime ECS. Different
component storage policies are intentional:

- Hot movement and rotation components may later use globally dense, contiguous SoA storage for
  one-pass batched/SIMD processing across entity types.
- Cold mandatory components can use direct fixed ranges based on entity capacities.
- Components requiring independent dense lifetime or ordering may use generated component-index
  maps plus a reverse stable `EntityId` owner to repair mappings after swap-remove.
- Small qualitative/query data can live in per-instance handles, refreshed at phase boundaries
  to avoid unnecessary component reads during Thinking.

Generated mappings must support batch resolution. Do not introduce scalar lookup facades as the
default processing model. These future storage migrations are separate work; the current change
does not move movement, rotation, or AI onto new component storage.
