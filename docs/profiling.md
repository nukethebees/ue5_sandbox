# Profiling

## Native level benchmarks with Tracy

Build the Development benchmark with localhost, on-demand Tracy support.

```powershell
cmake --preset native-benchmark
cmake --build --preset native-simulation-benchmark
```

Open Tracy Profiler, then run the executable directly for the profiler connection wait.

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

Connect Tracy to the local benchmark during the 60-second wait. After exit, check
`environment.tracy_enabled` in the JSON result and save the capture from Tracy as
`.local/benchmarks/<name>.tracy`.

## SandboxISMC Insights captures

Run `coj benchmark sandbox-ismc` or `coj benchmark sandbox-ismc-revision-ab`.
Each run writes `capture.utrace`, `metrics.csv`, `result.json`, and `unreal.log` to its run directory.
Use the manifest to check the trace path, source revision, workload, and requested and observed viewport.
Pass `--trace 0` to disable capture for the run or both revisions. A missing requested trace fails the run.

[SandboxISMC options](benchmarks.md#sandboxismc-experiments) · [Benchmark workloads](benchmarks.md) · [Level scripts](../LevelScripts/README.md)
