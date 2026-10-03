# Profiling

## Native level benchmarks with Tracy

The native level benchmark presets use the Development configuration, which compiles Tracy support
into the benchmark executable. Tracy is configured on demand and for localhost only: a benchmark
does not collect a trace or write a `.tracy` file unless a Tracy client connects while it is
running. The JSON result reports `environment.tracy_enabled`; confirm that it is `true` before
profiling.

Build the benchmark before reserving the machine for a measurement:

```powershell
cmake --preset native-benchmark
cmake --build --preset native-simulation-benchmark
```

Open the Tracy Profiler desktop application, request an exclusive jobs-board ticket, check until
Ready, and explicitly start it before running the benchmark. End it immediately when the command
returns. The profiler connection wait keeps the timed workload from starting until the profiler
connects (or the timeout expires).

```powershell
$repo = (Get-Location).Path
$benchmark = Join-Path $repo 'out\build\native-benchmark\bin\native-simulation-benchmark.exe'

& $benchmark `
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

`coj benchmark native-simulation` do not currently expose
`--wait-for-profiler`. They are appropriate for unprofiled measurements; use the
executable above when a reliable Tracy capture is required.

## SandboxISMC Insights captures

Use `coj benchmark sandbox-ismc` or `sandbox-ismc-revision-ab` for owned Insights captures.
Each process writes `capture.utrace` alongside `metrics.csv`, `result.json`, and `unreal.log` in
its allocated run directory. Trace capture defaults on; `--trace 0` disables it consistently for
both revisions. A requested but absent trace fails the run. The manifest records its exact path,
source identity, workload controls, and requested/observed viewport. See the
[SandboxISMC experiment options](benchmarks.md#sandboxismc-experiments).

## Related documentation

- [Benchmarks](benchmarks.md): supported benchmark workloads and runners.
- [Level scripts](../LevelScripts/README.md): find S7 benchmark scenarios.
- [Jobs board](../tools/jobserver/README.md): cooperative FIFO shared/exclusive tickets.
