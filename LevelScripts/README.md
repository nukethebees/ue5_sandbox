# Level scripts

Copy an existing `.scm` scenario, give it a unique `:id`, and add its path to
`catalog.scm` or a campaign. Use symbols for IDs and quoted lists for positions.

```scheme
(export mission)
(define defaults (module-ref (import-module "Settings/base.scm") 'game-settings))
(define mission
  (level :id 'example :title "Example"
    :player 'player
    :settings (settings-layers defaults (settings :fighters (fighters :speed 3000)))
    :logic (logic
      (define success (entity-destroyed 'turret))
      (define decision
        (latch :when (any-of success (entity-destroyed 'player)) :value success))
      (on :when (latched-true decision) :do (win 'blue))
      (on :when (latched-false decision) :do (win 'red)))
    :entities (list
      (entity :id 'player :archetype 'player-fighter
        :team 'blue :position '(0 0 0))
      (entity :id 'turret :archetype 'static-turret :team 'red
        :position '(0 10000 0)))))
```

Each import evaluates a fresh module. Definitions are private unless listed in a top-level
`(export name ...)` declaration. Use `(export (rename local public))` to publish an alias.
Paths are relative to the script root. The catalog reads the `mission` export from levels
and the `campaign` export from campaigns.

Settings start from C++ defaults. For an editor-backed level, apply shared settings,
explicit editor overrides and mission overrides in that order.

```scheme
(export mission)
(define defaults (module-ref (import-module "Settings/base.scm") 'game-settings))
(define scene (module-ref (import-module "LevelName/editor-config.scm") 'scene))
(define mission
  (copy-with scene :settings
    (settings-layers defaults (level-ref scene :settings) (settings :fighters (fighters :speed 3000)))))
```

Mark actors for export under **Details → Level Authoring → Export**.
**Export Scene** updates placements and viewpoint in `editor-config.scm` and preserves its
explicit settings overrides. **Export Entity Geometry** updates the mesh bounds and attachment
points imported by `Settings/base.scm`. **Play** uses the saved files.
Use record accessors and functional updates to compose levels, for example
`(copy-with base :entities (append (level-ref base :entities) extras))`.
`copy-with` replaces whole fields without mutating the original record; missing `foo-ref`
properties are errors.
See [gameplay logic](docs/logic.md).

Run a scenario with `coj benchmark native-simulation --level LevelScripts/your-level.scm --seconds 10`.
See [the native guide](../native/README.md) and [benchmark guide](../docs/benchmarks.md)
for builds and profiling.
