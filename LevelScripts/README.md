# Level scripts

`LevelScripts/` contains S7-authored scenarios consumed by the native simulation and Unreal
integration layers.

Records use keyword properties, symbols for identifiers, and strings for text:

```scheme
(level :id 'example :title "Example"
  :teams '(blue red)
  :player 'player
  :mission (mission :mode 'kill-enemies :heroes '(player))
  :entities (list
    (entity :id 'player :archetype 'player-fighter :team 'blue
      :position '(0 0 0) :rotation '(0 -90 0))))
```

Quote literal lists such as vectors and IDs. Use `list` when members must be evaluated,
such as entity constructors or computed coordinates. Each keyword takes one value;
nested records retain their constructor, for example `:camera (camera ...)`.
Campaigns use `(campaign :id 'main :title "Main" :levels '(example next-level))`.
Shared helpers loaded through `load-script` can construct the same records procedurally.

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
