# Simulation architecture

The simulation uses explicit, compile-time systems and generated SoA containers. Entity identity,
active storage layout, and component lifetime are separate concerns.

See [phases and data validity](docs/phases-and-data-validity.md) for the publication and mutation
contract.

## Level ownership and phases

`LevelSim` owns the clock, entity ledger and tables, concrete entity simulations, collision,
expressions, latches, timers, authored events and telemetry. It remains nonmovable because those owners retain
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

Construction consumes all initialization data, including static geometry and telemetry metadata.
It creates the optional player, dispatches initial spawn events, publishes entity lookups
and the spatial index, begins telemetry in `LevelState::StableSetup`, then enters
`LevelState::Paused` and dispatches initial tick-end events, which may immediately stop the run.
Setup uses Preparation for population and Thinking for stable queries, then
resets the independent clock to tick zero and `BetweenTicks` before returning. Its phase enum
contains only the repeating tick cycle. Logic evaluation during a tick
follows published deaths and precedes `ResolutionCommit`, while entity row mappings are still valid.

The first dispatched terminal event and its execution tick remain available through `result()`.
Run completion records its timestamp, including explicit stops for reset and world teardown.
Telemetry can be queried repeatedly. History ends at the latest scheduled sample; querying a run
reads current totals without adding a final sample or consuming metadata. Reporting and navigation
belong to consumers, not the telemetry manager.
Winner and stop reason are derived from that event's payload. Result publication, telemetry and
persistence use this terminal decision; there are no per-team mission completion records.
Initial terminal effects also publish a result, without starting the stopped simulation.
Duration limits compile to timer-triggered stop events. Living-team counts do not end a run.
Battle launches without an explicit duration have no time limit. Benchmarks and simulation-only
runs expose an End button independently of camera setup; visual gameplay retains its pause-menu exit.

`LevelEventManager` maps expression roots to ranges of packed event handles. Triggers
retire only after a true sample, queueing their events once. False leaves them eligible. Generated kind groups route events
to tick-end or Preparation streams. Each stream retains its append-only event storage and advances
an independent read cursor in FIFO order. The first dispatched terminal event enters
`LevelState::Stopped` and stops catch-up and dispatch; later events remain stored and unconsumed.
Stopped levels append no further events and cannot resume. Pause retains pending spawns and
leaves telemetry recording. Preparation after
resume routes the retained spawn span to `LevelSpawnManager`. `pause()` only queues a request;
`advance()` consumes tick-end events before accumulating elapsed time and after each completed
tick. Paused calls process queued events without accumulating time; `start()` resumes directly.
Initial and delayed level spawns use the same event payloads and dispatch path. The event manager
owns those payloads; `LevelSim` routes each spawn event to `LevelSpawnManager`, which resolves
spawn groups and dispatches typed payload views privately.
Recurring carrier production remains separate from these one-shot events. Unreal editor proxies
export authored records and are removed before PIE startup. Runtime initialization consumes the
composed Scheme definition and extracts only Unreal resource and static-collision data.

Unreal lifecycle and presentation ownership are described in the
[SpaceGame architecture](../../Plugins/SpaceGame/ARCHITECTURE.md).

## Timer state

`LevelSim` owns `Timers` independently of expression storage. Stable timer handles identify
not-started, started or finished state. `StartTimer` event payloads own durations; the timer collection
stores no duration before dispatch. Starting creates a countdown once; repeated starts do nothing.
Resolution decrements active countdowns before evaluating expressions. Expiry marks the entry
finished and swap-removes the countdown and its owner handle. Finished handles
remain readable. Timers have no reset operation; a fresh level owns fresh timer state.

Level compilation allocates timer IDs, creates start events and connects timer observations
to triggers. Expression compilation only receives existing timer handles; it neither allocates
timers nor creates effects. Logic compilation belongs to the level compiler. `CompiledLevelEvents` holds the
resulting trigger and effect definitions directly; there is no separate schedule object.
The compiler fills gameplay fields directly in the builder's fresh `LevelSimInitData`, using
the configuration already supplied there. Failed compilation discards that construction input.
Initial events are queued during event-manager construction. Initialisation dispatches queued
Preparation events before publishing entity lookups, then drains tick-end events immediately
before returning. Timer starts therefore occur at tick zero without evaluating logic or advancing
countdowns. Terminal events retain their stopping behaviour during initialisation.
Starting a timer becomes visible on the next expression evaluation.

## Boolean samples and latches

`ExprManager` evaluates every leaf each Resolution, grouped by predicate kind, followed by a
single dependency-ordered composite pass. Boolean results are sampled storage: no activation,
retirement, reference counting or same-tick propagation. Proximity is a current-state predicate;
destruction and lifetime kills query the ledger. Signals become true only on publication dispatch.

`Latches` stores stable expression-input pairs and one byte per latch. Two bits hold a pending
capture and two hold the cached observation: unset, false or true. Bulk evaluation reads sampled
inputs and writes only pending captures. Commit copies pending to cached and swap-removes captured
handles from the active list without moving definitions or state. Both fields then agree.

The Resolution order is commit previous captures, tick timers, evaluate expressions, evaluate
active latches, collect triggers. Captures become observable in the next logic cycle, consistently
for direct observations and composites. Chained latches advance one capture per cycle. Runtime
state is owned independently by each `LevelSim` and reset through fresh construction.

Scheme authoring uses a scoped `logic` macro and handle constructors. Compilation validates the
graph and resolves symbolic entity/signal references before runtime. See
[logic authoring](../../LevelScripts/docs/logic.md).

## Entity lookup

`LevelEntityRef` is a generated 32-bit declaration index, with `0xffffffff` reserved for an
invalid reference. Compilation resolves authored symbolic IDs to these references. The spawn
manager owns a contiguous declaration-to-`EntityId` array; delayed entities retain invalid runtime
IDs until their spawn event executes. Death preserves that slot for historical expressions.

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
