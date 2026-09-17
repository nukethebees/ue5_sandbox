# Benchmarks

Benchmarks are opt-in measurements, not ordinary test runs. Complete build and setup work first,
then use the repository runner or benchmark CMake target so the per-user jobserver can wait for
older work and acquire exclusive benchmark and machine resources. Do not start measurements beside
an unmanaged Unreal build or Editor session.

Results are disposable local data unless a specific experiment says otherwise; write them beneath
`.local/benchmarks/` rather than committing them.

## Entry points

| Measurement | Entry point | Notes |
| --- | --- | --- |
| Fighter scheduling simulation | `Scripts/run-fighter-simulation-benchmark.ps1` | Default 2,000- and 4,000-fighter cases; writes JSON and summary CSV. |
| Generic native simulation | `Scripts/run-native-simulation-benchmark.ps1` | Shared jobserver-aware wrapper around `native-simulation-benchmark` for an S7 level. |
| Frame-memory level workload | `Scripts/run-frame-memory-level-benchmark.ps1` | Uses the batch benchmark scenario. |
| Revision A/B frame-memory comparison | `Scripts/run-frame-memory-level-revision-ab.ps1` | Creates and evaluates a baseline worktree. |
| Native SOA and kernel experiments | Scripts named `run-*-benchmark*` or `run-*-experiment*` | Pair their output with the matching `plot-*.py` utility. |
| Unreal-backed measurements | Benchmark CMake presets and commandlet targets | Presets are in `cmake/presets/*benchmarks.json`. |

Run the fighter benchmark with:

```powershell
pwsh -NoProfile -File Scripts/run-fighter-simulation-benchmark.ps1
```

Run the initial scaling matrix with:

```powershell
pwsh -NoProfile -File Scripts/run-fighter-simulation-benchmark.ps1 `
    -FighterCaps 1000,2000,4000,8000,16000
```

Run a specific S7 level through the generic level benchmark runner with:

```powershell
pwsh -NoProfile -File Scripts/run-native-simulation-benchmark.ps1 `
    -Level .\LevelScripts\BenchmarkFleet_10.scm `
    -Seconds 20
```

Pass `-FighterCaps`, `-Seconds`, `-WarmupSeconds`, `-SaturationTimeoutSeconds`, or
`-OutputDirectory` to change the workload or destination. Use `-SkipBuild` only after confirming
the release benchmark binary is current.

Fighter caps must be unique positive 32-bit integers. Results are emitted in the requested cap
order; `results.json` and `summary.csv` retain that order. The generic runner accepts
comma-separated `-FighterStressCaps` values, while the native `--fighter-stress-caps` option also
accepts separate values.

The fighter runner verifies population stability, attacking state, active firing, absent replacement
spawns, and frame-memory capacity before publishing results. Its outputs include raw details plus
mean, median, p95, p99, tick rate, and realtime factor.

When multiple fighter caps are requested, the runner executes them in one native process. A single
Tracy capture therefore contains every case, with a formatted top-level zone such as `Fighter
simulation benchmark: 4000 fighters` around each one.

The workload waits for exact fighter saturation, applies a post-saturation warm-up, then measures
steady-state simulation. It disables fighter laser damage and raises ship health so the population
remains stable while normal fighter behaviour and collision work remain active.

For scripts and result plotters, see the [Scripts guide](../Scripts/README.md). For jobserver
implementation details, see [tools/jobserver](../tools/jobserver/README.md).

## Related documentation

- [Profiling](profiling.md): capture native level benchmarks with Tracy.
- [Level scripts](../LevelScripts/README.md): find benchmark scenarios and other S7 levels.
- [Scripts](../Scripts/README.md): benchmark runners and analysis utilities.
