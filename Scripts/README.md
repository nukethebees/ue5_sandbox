# Repository scripts

`Scripts/` contains focused developer automation. It is not the primary build interface: use
`dev.ps1` and CMake workflows for routine setup, builds, tests, and Editor work.

## Script groups

- `run-*-benchmark*` and `run-*-experiment*`: benchmark runners. They acquire the jobserver before
  collecting timings; see [Benchmarks](../docs/benchmarks.md).
- `plot-*.py`: convert benchmark JSON, CSV, or logs into plots and summaries.
- `audit_module_migration.sh` and `check_space_game_layers.py`: read-only architecture and migration
  checks. See [AGENTS.md](AGENTS.md) for the migration-audit contract.
- `format-cpp.py`, `test_format_cpp.py`, and the other `test_*.py` files: formatting and script
  validation support.
- `sbx_mimalloc_symbols.py` and `soa_spacing_confirmation.py`: targeted native-analysis helpers.

Run scripts from the repository root unless their own help says otherwise. Python scripts use the
repository's supported Python environment; run Pyright when changing one. Benchmark outputs belong
under `.local/benchmarks/`.
