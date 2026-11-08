# Gameplay logic

Put rules inside the level's `:logic (logic ...)` form. It establishes an authoring registry;
constructors return handles. Ordinary `define` and `let*` bindings share those handles, so a
reused expression is evaluated once. Handles belong to one logic block. Forward references and
authored feedback cycles are not supported. Scheme executes when loading the level, not during
simulation.

```scheme
:logic (logic
  (define deadline (timer))
  (define success (entity-destroyed 'enemy))
  (define decision
    (latch :when (any-of success (timer-elapsed deadline)) :value success))
  (initially (start-timer deadline 30))
  (on :when (latched-true decision) :do (win 'blue))
  (on :when (latched-false decision) :do (win 'red)))
```

Every Resolution samples all Boolean expressions after death publication. Previous latch captures
are committed first. Latches then capture from that sample, and eligible triggers queue effects.
A new capture is visible in the next logic cycle. In the example, simultaneous success and timeout
capture success because `:value` is `success`. Both latch observations are false before capture.
Latches capture once and remain readable for the rest of the run.

## Expressions

- `all-of` and `any-of` accept any number of handles. Empty AND is true; empty OR is false.
- `not-of` takes one handle. `xor-of` is variadic odd parity; empty XOR is false.
- `(entity-destroyed 'id)` is false before spawn and while alive, then permanently true after death.
- `(entity-reached-position 'id '(x y z) radius)` tests the current sampled position of a live
  entity. It becomes false after departure or death. Use a latch to remember an arrival.
- `(entity-kills-reached '(entity-ids...) count)` sums lifetime credited kills of distinct listed
  entities, including dead entities. Unspawned entities contribute zero. This is not a team query.
- `(timer-elapsed timer-handle)` is false until a started timer reaches zero, then permanently true.
- `(latched-true latch-handle)` and `(latched-false latch-handle)` observe committed captures.
- `(event-fired 'signal)` is permanently true after `(publish 'signal)` has dispatched. Queueing
  publication does not change the current expression sample.

## Effects and timing

`(on :when expression :do effect)` fires once on its first true sample. False keeps it eligible.
Use `:do (list effect ...)` for multiple effects. `(initially effect ...)` queues setup effects,
which dispatch before initialization finishes, without evaluating logic or advancing the clock.

`(timer)` creates a stopped timer. `(start-timer timer-handle seconds)` carries the duration;
durations round up to simulation ticks. Repeated starts do nothing. Timers count simulation time,
so pausing stops them. A timer started by a rule becomes observable on subsequent evaluations.

Declare reinforcements among the level's entities, then explicitly group them:

```scheme
:logic (logic
  (define reserves (spawn-group 'reserves '(reserve-a reserve-b)))
  (on :when (entity-destroyed 'flagship)
      :do (list (spawn reserves) (publish 'reserves-requested))))
```

Grouped entities do not spawn automatically. An entity belongs to at most one group; grouped
entities spawn once. Later requests do nothing, including after the original entities die. Grouped
entities cannot be players or also have `:spawn-at`. Ungrouped entities retain automatic time-zero
or delayed spawning. Spawn effects dispatch in Preparation; other effects dispatch at tick end.
FIFO applies within each stream. A signal alongside a spawn indicates its own publication, not
that the spawn has dispatched.

`(win 'team)` and `(stop)` end the level when dispatched. The first terminal event determines the
result; later queued events remain stranded. A winner implies defeat for other participating teams.
Stopping without a winner records the reason without inventing a winner. A fresh level construction
resets timers, latches, signals, rules and history.

`Libraries/logic.scm` provides `destroy-all`, `destroy-any`, `level-delay` and `win-race` as ordinary
Scheme helpers. The editor owns only its generated configuration; keep gameplay logic in the human
root. Serialization preserves graph data and sharing, not the original Scheme source formatting.
