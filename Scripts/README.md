# Repository scripts

`Scripts/` contains focused developer automation. It is not the primary build interface: use
agent-task for developer operations and CMake for builds and ordinary tests.

## Script groups

- `agent-task benchmark <operation>` builds and invokes revision-local `benchmark-tools`;
  workload execution and reporting stay in that Rust tool.
- `plot-*.py`: plotting and scientific presentation only.
- `test_*.py` files: focused Python script validation support. Repository C++ and shader
  formatting uses centrally installed `agent-task format` from PATH with `.code-format.json` policy.
- Mimalloc object-symbol analysis is provided by `native-binary-tools` under `tools/rust/`.

Run scripts from the repository root unless their own help says otherwise. Rust owns benchmark
orchestration, PowerShell is shell glue, and Python remains for plotting and scientific analysis.
Python scripts use the repository's supported Python environment; run Pyright when changing one.
Benchmark outputs belong under `.local/benchmarks/`.
