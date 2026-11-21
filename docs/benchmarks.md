# Benchmarks

Build first. Keep all measurement runs within three minutes and avoid competing workloads. Warm up before sampling. Results go under `.local/benchmarks/`.

## Compare revisions

From the candidate checkout, compare local edits against `dev`:

```powershell
coj benchmark compare --baseline dev --prepare-only
coj benchmark compare --baseline dev --skip-build --baseline-worktree <printed-baseline>
```

Replace the placeholder with the path printed by preparation. For two commits, pass `--candidate <ref>` to both commands and `--candidate-worktree <printed-candidate>` to the second. Use identical workload options both times. Read `comparison.md` in the printed run directory. Lower tick time and higher throughput favor the candidate.

## Run one workload

| Measure | Command |
| --- | --- |
| Fighter tick time at 2,000 and 4,000 fighters | `coj benchmark fighter-simulation` |
| One scripted level | `coj benchmark native-simulation --level LevelScripts/BenchmarkFleet_10/BenchmarkFleet_10.scm --seconds 20` |
| Frame memory | `coj benchmark frame-memory-level --seconds 20` |
| Unreal instances | `coj benchmark sandbox-ismc --instances 1000 --warmup-seconds 1 --seconds 2` |

For Unreal instance runs, enable `SandboxISMCLab` in `Sandbox.uproject` and rebuild the editor in each revision. Use `sandbox-ismc-revision-ab` for comparisons. Inspect `metrics.csv`, `result.json`, and the Insights `capture.utrace` in the printed directory.

## Capture a native Tracy trace

Open Tracy Profiler, then run from the repository root:

```powershell
cmake --preset native-benchmark
cmake --build --preset native-simulation-benchmark
.\out\build\native-benchmark\bin\native-simulation-benchmark.exe --level LevelScripts/FighterSchedulingBenchmark/FighterSchedulingBenchmark.scm --seconds 20 --fighter-stress-cap 2000 --warmup-seconds 5 --saturation-timeout-seconds 60 --wait-for-profiler 60
```

Connect to the waiting local process. After it exits, save the capture from Tracy as `.local/benchmarks/run.tracy`. With a compatible `tracy-csvexport` on PATH, locate the steady-state interval:

```powershell
coj benchmark tracy-report --trace .local/benchmarks/run.tracy --filter "Benchmark measured ticks"
```

Run the report again with a subsystem `--filter` and the interval's `--from-seconds` and `--to-seconds`. Compare the same window and workload for subsystem probes and full ticks.
