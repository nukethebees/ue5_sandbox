# Level scripts

`LevelScripts/` contains S7-authored scenarios consumed by the native simulation and Unreal
integration layers.

Records use keyword properties, symbols for identifiers, and strings for text:

```scheme
(level :id 'example :title "Example"
  :player 'player
  :mission (mission :mode 'kill-enemies :heroes '(player))
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 -90 0))))
```

Teams are derived from entity assignments, including scheduled spawns; no team list is declared.
Entity `:rotation` is optional and defaults to `(0 0 0)`. Entity `:id` is also optional:
the parser assigns `<team>-<archetype>-<number>`, such as `red-static-turret-0`.
Numbering starts at zero for each team/archetype within each level, follows entity order,
and skips all explicit IDs, including those on later entities. Use explicit IDs for units
referenced by the player, camera, or mission: generated names can change when a group is edited.

Use `map` to share entity properties across a formation:

```scheme
(map (lambda (position)
       (entity :archetype 'static-turret :team 'red :position position))
     '((-12000 10000 -2500)
       (-8000 6000 2500)
       (-4000 10000 2500)))
```

For evenly spaced formations, map over indices and calculate coordinates with expressions
such as `(* index 4000)`. Explicit IDs can also be constructed with
`(string->symbol (format #f "flagship-~A" index))`. Combine groups with `append`, or use
`cons` to prepend a single player entity. Keep level-specific helpers inside `let`;
put helpers shared by several levels in `Libraries/`.

Editor saves expand procedural definitions into explicit entity records and preserve every
resolved ID. They omit zero rotations; they do not preserve the original loops.

Quote literal lists such as vectors and IDs. Use `list` when members must be evaluated,
such as entity constructors or computed coordinates. Each keyword takes one value;
nested records retain their constructor, for example `:camera (camera ...)`.
Campaigns use `(campaign :id 'main :title "Main" :levels '(example next-level))`.
The game loads `catalog.scm` as its root. It selects files explicitly:

```scheme
(load-script "Libraries/helpers.scm")
(catalog
  :levels '("Levels/intro.scm" "Levels/battle.scm")
  :campaigns '("Campaigns/main.scm"))
```

The example paths are illustrative. Each catalog property accepts a list of filename strings;
use ordinary Scheme expressions to construct variations. Files are evaluated in list order,
and catalog properties are processed in the order written. Nothing is discovered automatically.
All imports, including imports inside another file, are relative to the root file's directory.
`load-script` evaluates an explicit helper or selection file and returns its last expression;
successful loads are cached for that interpreter, including their return values.

A root and all its imports share one interpreter. Loading another root starts fresh. The
evaluated catalog is copied into an owned flat AST, then the interpreter is destroyed before
native parsing and validation. Source filenames are retained for diagnostics and editor saves.
Each selected definition reports its own evaluation/parsing errors; failures in the root itself
fail the root load. Scheme assignments made before an error are not rolled back.

- `Campaigns/` contains gameplay, showcase, evaluation, and development scenarios.
- `Benchmarks/` contains deterministic workloads used by benchmark runners.
- `Libraries/` contains shared S7 support code.

The top-level `.scm` scenarios include the fighter scheduling benchmark and development/test cases.
`Benchmarks/` contains the deterministic batch workload. Use the shared runner to time a specific
level rather than launching a timing workload manually:

```powershell
.\out\build\native\rust-tools\release\benchmark-tools.exe native-simulation `
  --level .\LevelScripts\BenchmarkFleet_10.scm `
  --seconds 20
```

See [Benchmarks](../docs/benchmarks.md) for the focused fighter and frame-memory runners, and
[Profiling](../docs/profiling.md) to capture a native level benchmark with Tracy. For S7 and native
scenario support, start with the [native guide](../native/README.md).
