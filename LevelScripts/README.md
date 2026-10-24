# Level scripts

Copy an existing `.scm` scenario, give it a unique `:id`, and add its path to
`catalog.scm` or a campaign. Use symbols for IDs and quoted lists for positions.

```scheme
(level :id 'example :title "Example"
  :player 'player
  :settings (fighters :speed 3000)
  :mission (mission
    :objectives (list
      (objective :id 'clear :team 'blue :title "Clear the range"
        :pass (entity-destroyed 'turret)
        :fail (entity-destroyed 'player)))
    :team-results (list
      (team-result :team 'blue :when (objective-resolved 'clear 'passed) :result 'victory)
      (team-result :team 'blue :when (objective-resolved 'clear 'failed) :result 'defeat)))
  :entities (list
    (entity :id 'player :archetype 'player-fighter
      :team 'blue :position '(0 0 0))
    (entity :id 'turret :archetype 'static-turret :team 'red
      :position '(0 10000 0))))
```

All game defaults live in `Settings/base.scm`. A level overrides only the fields
it supplies. For a separate settings file, compose defaults and overrides in order:

```scheme
(settings-layers
  (load-script "Settings/base.scm")
  (fighters :speed 3000))
```

Imports are relative to `LevelScripts/`. Shared formation helpers live in
`Libraries/`. Assets, colours and scene bindings stay in Unreal properties.
Edit scripts directly for missions, settings and entity overrides; editor scene saving
rejects these unsupported features. See [objective authoring](docs/objectives.md).

Run a scenario with `coj benchmark native-simulation --level LevelScripts/your-level.scm --seconds 10`.
See [the native guide](../native/README.md) and [benchmark guide](../docs/benchmarks.md)
for builds and profiling.
