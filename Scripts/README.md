# Repository scripts

`Scripts/` contains focused developer automation. It is not the primary build interface: use
`dev.ps1` and CMake workflows for routine setup, builds, tests, and Editor work.

## Script groups

- The focused PowerShell benchmark names are interactive façades only: they stage
  `BenchmarkTools.exe` if needed and forward their arguments. Benchmark execution, validation,
  filesystem work, and jobserver claims live in BenchmarkTools.
- `plot-*.py`: plotting and scientific presentation only.
- `audit_module_migration.sh`: read-only migration checks. See [AGENTS.md](AGENTS.md) for the
  migration-audit contract.
- `test_*.py` files: focused Python script validation support. Repository C++ and shader
  formatting is provided by the C# `CodeFormatTools` developer tool under `tools/`.
- Mimalloc object-symbol analysis is provided by the `NativeBinaryTools` C# tool under `tools/`.

Run scripts from the repository root unless their own help says otherwise. C# owns benchmark
orchestration, PowerShell is shell glue, and Python remains for plotting and scientific analysis.
Python scripts use the repository's supported Python environment; run Pyright when changing one.
Benchmark outputs belong under `.local/benchmarks/`.
