# Objective authoring

A mission contains `:objectives`. Each objective requires a
unique `:id`, a `:team`, and at least one of `:pass`, `:fail`, or `:expire`. `:title` is optional.
`:wins-level` defaults to `'no`. Passing an objective with `:wins-level 'yes` wins the level
for its assigned team and defeats the other participating teams. Failed and expired objectives
do not directly decide the mission. To make failure decisive, give an opposing team a winning
objective that observes that failure. The player's team determines the level completion screen.

```scheme
(mission
  :objectives (list
    (objective :id 'approach :team 'blue
      :pass (entity-reached-position 'player '(0 10000 0) 500)
      :fail (entity-destroyed 'player))
    (objective :id 'hold :team 'blue
      :wins-level 'yes
      :depends-on (list (objective-resolved 'approach 'passed))
      :pass (time-elapsed 30)
      :fail (any-of (entity-destroyed 'player) (entity-destroyed 'convoy)))
    (objective :id 'bonus :team 'blue
      :pass (entity-destroyed 'enemy)
      :expire (objective-resolved 'hold 'passed))
    (objective :id 'opposition :team 'red :wins-level 'yes
      :pass (any-of (objective-resolved 'approach 'failed)
                    (objective-resolved 'hold 'failed)))))
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
within each notification wave. The first evaluated passing objective marked `:wins-level 'yes`
decides the winner; later objective completions cannot change the mission outcome.

Load `Libraries/objectives.scm` for `destroy-all`, `destroy-any`, `decisive-mission`, and
`reciprocal-objectives`. Use `(decisive-mission team opponent pass fail)` for a primary winning
objective and an opposing winning objective that observes its failure. The reciprocal helper
returns attacker/defender objectives, each marked `:wins-level 'yes`.

## Current presentation and persistence limits

The HUD shows active objective titles and lifecycle counts. Arbitrary expressions do not imply
unique defend/destroy markers or a single countdown, so those legacy displays are removed.
The old mission-global stopwatch is hidden; objective timers still use simulation time.
The scene editor rejects mission graph editing and saving; edit the S7 source directly.
Programmatic proxy missions can bind authored entity names to actors, but legacy map mission
mode properties are no longer supported. Migrate such missions to S7 or the generic definition.
Old mission result save versions and telemetry schemas are rejected explicitly. New results
store team outcomes and decisive objective identifiers; existing files are not rewritten.
