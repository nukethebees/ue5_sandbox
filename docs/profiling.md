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

The convenience PowerShell level runners do not currently expose `--wait-for-profiler`. They are
appropriate for unprofiled measurements; use the jobserver-wrapped executable above when a
reliable Tracy capture is required.

## Related documentation

- [Benchmarks](benchmarks.md): supported benchmark workloads and runners.
- [Level scripts](../LevelScripts/README.md): find S7 benchmark scenarios.
- [Jobserver](../tools/jobserver/README.md): resource claims and command supervision.
