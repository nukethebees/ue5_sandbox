# Level scripts

`LevelScripts/` contains S7-authored scenarios consumed by the native simulation and Unreal
integration layers.

- `Campaigns/` contains gameplay, showcase, evaluation, and development scenarios.
- `Benchmarks/` contains deterministic workloads used by benchmark runners.
- `Libraries/` contains shared S7 support code.

The top-level `.scm` scenarios include the fighter scheduling benchmark and development/test cases.
Use benchmark scripts rather than launching a timing workload manually; see [Benchmarks](../docs/benchmarks.md).
For S7 and native scenario support, start with the [native guide](../native/README.md).
