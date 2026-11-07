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

Construction creates the optional player and queues initial events, leaving the level in
`LevelState::Uninitialised`. The host may install static geometry before calling
`finish_initialisation()`, which dispatches initial spawn events and publishes entity lookups
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
Battle launches without an explicit duration have no time limit. Benchmarks and simulation-only
runs expose an End button independently of camera setup; visual gameplay retains its pause-menu exit.

`LevelEventManager` maps expression roots to ranges of packed event handles. Resolved triggers
retire after Resolution, queueing their events once when true. Generated kind groups route events
to tick-end or Preparation streams. Each stream retains its append-only event storage and advances
an independent read cursor in FIFO order. The first dispatched terminal event enters
`LevelState::Stopped` and stops catch-up and dispatch; later events remain stored and unconsumed.
Stopped levels append no further events and cannot resume. Pause retains pending spawns and
leaves telemetry recording. Preparation after
resume routes the retained spawn span to `LevelSpawnManager`. `pause()` only queues a request;
`advance()` consumes tick-end events before accumulating elapsed time and after each completed
tick. Paused calls process queued events without accumulating time; `start()` resumes directly.
Initial and delayed level spawns use the same event payloads and dispatch path. The event manager
owns those payloads; `LevelSim` resolves spawn groups and passes typed views to `LevelSpawnManager`.
Recurring carrier production remains separate from these one-shot events. Unreal proxy binding
occurs after initial event dispatch, before the proxies are destroyed; static collision extraction
excludes those retained source actors.

Unreal lifecycle and presentation ownership are described in the
[SpaceGame architecture](../../Plugins/SpaceGame/ARCHITECTURE.md).

## Timer state

`LevelSim` owns `Timers` independently of expression storage. Stable timer handles identify
internal 32-bit entries: two bits for not-started, started or finished state, and thirty bits
for an active countdown index. `StartTimer` event payloads own durations; the timer collection
stores no duration before dispatch. Starting creates a countdown once; repeated starts do nothing.
Resolution decrements active countdowns before evaluating expressions. Expiry marks the entry
finished and swap-removes the countdown, repairing the moved owner's index. Finished handles
remain readable. Timers have no reset operation; a fresh level owns fresh timer state.

Level compilation allocates timer IDs, creates start events and connects timer observations
to triggers. Expression compilation only receives existing timer handles; it neither allocates
timers nor creates effects. Mission compilation also belongs to the level compiler, keeping
runtime `Mission` independent of timer and event storage. `CompiledLevelEvents` holds the
resulting trigger and effect definitions directly; there is no separate schedule object.
Initial events are queued during event-manager construction. Initialisation dispatches queued
Preparation events before publishing entity lookups, then drains tick-end events immediately
before returning. Timer starts therefore occur at tick zero without evaluating logic or advancing
countdowns. Terminal events retain their stopping behaviour during initialisation.
Objective activation queues timer-start events for tick-end dispatch. Expression activation
only schedules evaluation and does not start or restart timers. Expressions retain the current
`Pending`/`True` semantics until the
Boolean conversion; starting a timer becomes visible on the next expression evaluation.

## Latch state

`Latches` stores stable expression-input pairs and one byte of state per latch. Its low two bits
hold a pending capture and the next two hold the cached observation: unset, false or true.
Bulk evaluation reads each active latch's sampled inputs directly from `ExprManager` and writes
only the pending field, once. While expressions still use `ExprResult`, only `True` is satisfied.
Commit publishes captures and swap-removes their handles from the active list without moving latch
definitions or state. Both fields then agree. Observations always read the cached field.

This is the runtime state foundation. Expression observations and `LevelSim` integration follow
with the Boolean evaluator conversion; the existing retiring expression evaluator is unchanged.
The integration order is commit previous captures, evaluate expressions, evaluate active latches,
then collect triggers. Captures become observable in the following logic cycle.

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
