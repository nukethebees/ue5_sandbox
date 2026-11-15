# Simulation architecture

The simulation uses concrete systems, explicit dependencies, index-based handles and generated
SoA storage. See [phases and data validity](docs/phases-and-data-validity.md) for publication,
mutation and scratch-lifetime contracts.

## Ownership

`LevelSim` owns the clock, memory resources, entity ledger and tables, entity simulations,
collision pipeline, `ConditionManager`, `Timers`, `LevelEventManager` and `LevelSpawnManager`.
It coordinates phases and remains nonmovable because subsystems borrow other owners.
Declaration order keeps those dependencies alive until their consumers are destroyed.

- `ConditionManager` owns Boolean expressions, sampled results and persistent latch state.
- `Timers` owns countdown state independently of conditions; start-event payloads own durations.
- `LevelEventManager` owns event payloads, trigger definitions, active trigger indices and phase
  queues. It dispatches events using dependencies supplied by `LevelSim`.
- `LevelSpawnManager` owns authored-to-runtime entity mappings, carrier cooldowns and spawn
  transforms. It borrows destination simulations per call. Fighter spawn requests remain in the
  fighter simulation until the next Preparation.
- `CollisionPipeline` owns spatial queries, overlap response and reusable candidate identities.
  It captures spawn identities before row reordering and refreshes the index for collision queries.

Level construction fills `LevelSimInitData` and queues startup events directly in the event
manager. Definitions own their storage; moving the manager into `LevelSim` transfers queue
contents to level memory while preserving read cursors. Tick and phase scratch must not escape
their respective lifetimes.

## Conditions and timing

Resolution publishes deaths, evaluates mission logic while entity mappings remain valid, then
commits entity removals. Logic runs in this order:

1. Commit the previous tick's pending latch captures.
2. Advance timers.
3. Evaluate expression leaves in predicate batches, then composites in dependency order.
4. Capture enabled latch inputs for the following tick.
5. Collect ready triggers.

`ConditionManager` performs steps 1, 3 and 4; `LevelSim` advances timers between its calls.
Latches capture once. Every expression observes committed state, so chained latches advance
one capture per cycle. Expression results remain current samples: proximity can become false,
while destruction and lifetime kills use ledger history.

Timers start once; repeated starts do nothing. Finished handles remain readable. Timer-start
events execute at tick end and become visible to expressions on their next evaluation.
Fresh level construction resets conditions and timers.

## Events and dispatch

Event definitions and triggers can be appended during construction or simulation. A trigger
stores a condition handle and a range in the flat event-handle array. Active indices preserve
registration order. A true condition queues the associated events once and retires only the
active index. Triggers registered during collection become eligible on the next pass.

Preparation and tick-end queues preserve FIFO order independently. Preparation dispatches
spawns; tick-end dispatches timer starts, pauses and terminal events. A terminal event stops
tick-end dispatch immediately, retains later queued events, and records its handle and tick
for `result()`. Stopped levels do not advance or resume.

Construction dispatches queued spawns, publishes lookups and the spatial index, resets the clock
to tick zero and `BetweenTicks`, then dispatches tick-end events from the paused state.
It does not evaluate conditions or advance timers; a queued terminal event can stop the level
during construction.

`advance()` dispatches tick-end events before accumulating time and after each completed tick.
Paused calls process that queue without accumulating time. Pause retains pending spawns for
Preparation after resume. Duration limits use timer-triggered stop events.

## Entity identity and lookup

`LevelEntityRef` identifies an authored declaration. The spawn manager maps it to a persistent
`EntityId`; delayed entities remain unresolved until spawned. Death preserves this mapping and
ledger history.

`EntityId` identifies an entity type and lifetime offset. Lookup tables resolve it to the current
active row, team and quantised health. Row handles and borrowed views must not survive structural
mutation. Resolve queries in batches by entity type, with caller-owned scratch.

Authoritative health uses type-specific storage indexed by active row. Packed lookup health is
a Thinking snapshot rebuilt during Preparation.

See [logic authoring](../../LevelScripts/docs/logic.md) for Scheme definitions and the
[SpaceGame architecture](../../Plugins/SpaceGame/ARCHITECTURE.md) for Unreal lifecycle and presentation.
