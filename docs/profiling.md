# Profiling

## Native level benchmarks with Tracy

The native level benchmark presets use the Development configuration, which compiles Tracy support
into the benchmark executable. Tracy is configured on demand and for localhost only: a benchmark
does not collect a trace or write a `.tracy` file unless a Tracy client connects while it is
running. The JSON result reports `environment.tracy_enabled`; confirm that it is `true` before
profiling.

Build the benchmark before reserving the machine for a measurement:

```powershell
cmake --preset native-simulation-benchmark
cmake --build --preset native-simulation-benchmark
```

Open the Tracy Profiler desktop application, then use the jobserver to run the benchmark with a
connection wait. The wait keeps the timed workload from starting until the profiler connects (or
the timeout expires).

```powershell
$repo = (Get-Location).Path
$jobserver = Join-Path $env:LOCALAPPDATA 'NukeTheBees\jobserver\bin\jobserver.exe'
$benchmark = Join-Path $repo 'out\build\native-simulation-benchmark\bin\native-simulation-benchmark.exe'

& $jobserver run `
  --name 'fighter scheduling Tracy capture' `
  --kind benchmark `
  --worktree $repo `
  --exclusive machine `
  --exclusive benchmark `
  -- $benchmark `
  --level (Join-Path $repo 'LevelScripts\FighterSchedulingBenchmark.scm') `
  --seconds 20 `
  --fighter-stress-cap 2000 `
  --warmup-seconds 5 `
  --saturation-timeout-seconds 60 `
  --wait-for-profiler 60
```

While the executable is waiting, connect the Tracy Profiler to the local benchmark process. After
the benchmark exits, save the capture from the Tracy Profiler to a `.tracy` file; the client does
not save one automatically. Store local captures beneath `.local/benchmarks/` and do not commit
them.

`BenchmarkTools native-simulation` and its PowerShell report wrappers do not currently expose
`--wait-for-profiler`. They are appropriate for unprofiled measurements; use the jobserver-wrapped
executable above when a reliable Tracy capture is required.

## Comparing two native benchmark configurations

`tracy-benchmark-compare` captures the same level under two CMake presets, exports Tracy's
inclusive and self-time zone CSVs, and writes a structured comparison beneath
`.local/benchmarks/tracy-comparison/`. Build it with the Tracy tools:

```powershell
cmake --workflow --preset tracy-tools
```

The worktree executable is `out\build\tracy-tools\bin\tracy-benchmark-compare.exe`. It requires
the worktree root explicitly so an installed copy can safely measure any checkout:

```powershell
.\out\build\tracy-tools\bin\tracy-benchmark-compare.exe `
  --root (Get-Location) `
  --level .\LevelScripts\FighterSchedulingBenchmark.scm `
  --seconds 20 `
  --a-preset native-simulation-benchmark `
  --b-preset native-simulation-benchmark
```

The command builds missing prerequisites unless `--skip-build` is supplied, obtains the benchmark
and machine jobserver claims, then records `manifest.json`, `comparison.json`, both captures, and
their exported CSVs. Install a validated version independently of a worktree with:

```powershell
cmake --install .\out\build\tracy-tools --component tracy-benchmark-compare --prefix $env:LOCALAPPDATA\NukeTheBees\perf-tools
```

## Related documentation

- [Benchmarks](benchmarks.md): supported benchmark workloads and runners.
- [Level scripts](../LevelScripts/README.md): find S7 benchmark scenarios.
- [Jobserver](../tools/jobserver/README.md): resource claims and command supervision.
