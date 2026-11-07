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
        :wins-level 'yes
        :pass (entity-destroyed 'turret)
        :fail (entity-destroyed 'player))
      (objective :id 'opposition :team 'red :wins-level 'yes
        :pass (objective-resolved 'clear 'failed))))
  :entities (list
    (entity :id 'player :archetype 'player-fighter
      :team 'blue :position '(0 0 0))
    (entity :id 'turret :archetype 'static-turret :team 'red
      :position '(0 10000 0))))
```

Settings resolve from C++ defaults, then `Settings/base.scm`, then the level's
`:settings`. A level overrides only the fields it supplies, including collision-grid
dimensions. For a separate settings file, compose overrides in order:

```scheme
(settings-layers
  (load-script "Settings/base.scm")
  (fighters :speed 3000))
```

Imports are relative to `LevelScripts/`. Shared formation helpers live in
`Libraries/`. Assets, colours and scene bindings stay in Unreal properties.
Editor exports write only `LevelName/editor-config.scm`; the human-owned `LevelName.scm`
loads and extends that baseline. The generated baseline need not be playable on its own.
Use record accessors and functional updates to compose levels, for example
`(level-with base :entities (append (level-ref base :entities) extras))`.
`foo-with` replaces whole fields without mutating the original record; missing `foo-ref`
properties are errors. Only proxies marked **Export** are serialized by the editor.
See [objective authoring](docs/objectives.md).

Run a scenario with `coj benchmark native-simulation --level LevelScripts/your-level.scm --seconds 10`.
See [the native guide](../native/README.md) and [benchmark guide](../docs/benchmarks.md)
for builds and profiling.
