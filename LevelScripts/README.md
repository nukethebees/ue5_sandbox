# Level scripts

`LevelScripts/` contains S7-authored scenarios consumed by the native simulation and Unreal
integration layers.

- `Campaigns/` contains gameplay, showcase, evaluation, and development scenarios.
- `Benchmarks/` contains deterministic workloads used by benchmark runners.
- `Libraries/` contains shared S7 support code.

The top-level `.scm` scenarios include the fighter scheduling benchmark and development/test cases.
`Benchmarks/` contains the deterministic batch workload. Use the shared runner to time a specific
level rather than launching a timing workload manually:

```powershell
.\tools\bin\BenchmarkTools.exe native-simulation `
  --level .\LevelScripts\BenchmarkFleet_10.scm `
  --seconds 20
```

See [Benchmarks](../docs/benchmarks.md) for the focused fighter and frame-memory runners, and
[Profiling](../docs/profiling.md) to capture a native level benchmark with Tracy. For S7 and native
scenario support, start with the [native guide](../native/README.md).
