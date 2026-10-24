# Objective authoring

A mission contains `:objectives` and optional `:team-results`. Each objective requires a
unique `:id`, a `:team`, and at least one of `:pass`, `:fail`, or `:expire`. `:title` is optional.
Passing an ordinary objective does not end the level. A team-result explicitly maps an
objective outcome to that team's `victory` or `defeat`; the player's team determines the
level completion screen.

```scheme
(mission
  :objectives (list
    (objective :id 'approach :team 'blue
      :pass (entity-reached-position 'player '(0 10000 0) 500)
      :fail (entity-destroyed 'player))
    (objective :id 'hold :team 'blue
      :depends-on (list (objective-resolved 'approach 'passed))
      :pass (time-elapsed 30)
      :fail (any-of (entity-destroyed 'player) (entity-destroyed 'convoy)))
    (objective :id 'bonus :team 'blue
      :pass (entity-destroyed 'enemy)
      :expire (objective-resolved 'hold 'passed)))
  :team-results (list
    (team-result :team 'blue :when (objective-resolved 'hold 'passed) :result 'victory)
    (team-result :team 'blue :when (objective-resolved 'approach 'failed) :result 'defeat)
    (team-result :team 'blue :when (objective-resolved 'hold 'failed) :result 'defeat)))
```

Conditions and composition:

- `(all-of expressions...)` and `(any-of expressions...)` support any number of children.
  Empty `all-of` is true; empty `any-of` is false.
- `(entity-destroyed 'id)` becomes true when the entity dies, including deaths before activation.
  References to delayed spawns stay pending until they spawn and die.
- `(entity-reached-position 'id '(x y z) radius)` latches true at the first sampled arrival.
  If the entity dies before arrival is sampled, it becomes false. It uses simulation positions
  and world units, with a nonnegative radius.
- `(time-elapsed seconds)` starts at objective activation; durations round up to simulation ticks.
- `(objective-resolved 'id 'passed)` observes a published outcome. Outcomes are `passed`, `failed`,
  and `expired`. A different terminal outcome makes the condition false.
- `(entity-kills-reached '(source-ids...) count)` sums credited lifetime kills from the listed
  entities. Source IDs must be distinct; the target is explicit and nonnegative.

All activation dependencies must match before an objective becomes active. A mismatched
dependency leaves it inactive. Observation cycles may remain pending; expression DAG cycles
and activation-dependency cycles are rejected. False roots do not resolve objectives.
Completed objectives and expression results are terminal. No objective automatically cancels
another; use an explicit `:expire` expression to supersede it.

Share subexpressions within one objective using names:

```scheme
(objective :id 'shared :team 'blue
  :nodes (list (expr 'deadline (time-elapsed 60)))
  :pass (all-of (entity-destroyed 'enemy) (expr-ref 'deadline))
  :expire (all-of (objective-resolved 'bypass 'passed) (expr-ref 'deadline)))
```

Names are local to the objective; forward references are supported. Simultaneous true roots
use pass, then fail, then expire precedence. Objective transitions settle in declaration order
within each notification wave. Conflicting same-tick team mappings prefer defeat; team outcomes
then remain terminal.

Load `Libraries/objectives.scm` for `destroy-all`, `destroy-any`, `decisive-mission`, and
`reciprocal-objectives`. The reciprocal helper returns attacker/defender objectives; explicitly
add team mappings for the outcomes that should decide the mission.

## Current presentation and persistence limits

The HUD shows active objective titles and lifecycle counts. Arbitrary expressions do not imply
unique defend/destroy markers or a single countdown, so those legacy displays are removed.
The old mission-global stopwatch is hidden; objective timers still use simulation time.
The scene editor rejects mission graph editing and saving; edit the S7 source directly.
Programmatic proxy missions can bind authored entity names to actors, but legacy map mission
mode properties are no longer supported. Migrate such missions to S7 or the generic definition.
Old mission result save versions and telemetry schemas are rejected explicitly. New results
store team outcomes and decisive objective identifiers; existing files are not rewritten.
