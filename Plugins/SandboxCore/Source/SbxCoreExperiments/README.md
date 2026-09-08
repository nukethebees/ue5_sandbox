# Single-allocation generated SoA experiment

An opt-in generated owner with one aligned allocation, one size and one capacity for all 53 flattened columns. The default experimental Unreal owner uses mimalloc. Production SoAs remain TArray-backed and production fighter data is unchanged.

The [investigation report](INVESTIGATION.md) records the layout/lifetime design, generator architecture, allocator investigation, measured results, limitations, and instructions for recreating the full experiment. Historical CSVs and PNG plots are preserved in [results](results/).

## Quick comparison

Stop competing workloads, then run from the repository root:

```powershell
cmake --workflow --preset single-allocation-soa-benchmark
```

This builds the Development benchmark executable, runs correctness, and compares these two representations:

| Test suffix | Generated owner | Backing allocation |
|---|---|---|
| `reserve_TArray` | `EntityData` | 53 ordinary TArrays / Unreal allocator |
| `reserve_SingleMimalloc` | `SingleAllocationEntityData` | One direct mimalloc allocation |

Each reserve invocation constructs **one owner**, reserves **65,536 rows**, and destroys it. No rows are explicitly initialized. Catch2 calibrates repetitions and collects its default 100 samples; this is one reserve per invocation, not one timing sample. CTest launches each reserve implementation in a separate process and runs serially. There is no owner sweep or raw allocator matrix. This comparison measures both representation and allocator differences; it is not the same-allocator experiment in the report.

The full workflow also measures natural/reserved/defaulted append (batches of 1 and 64), populated growth, set-num grow/shrink, reset/reuse, swap removal, narrow/wide iteration and view construction. All use 65,536 rows and only TArray versus Single/Mimalloc. These operations remain separate tests for profiling; each compares both owners. Iteration reuses initialized storage, and reserved operations exclude initial reserve. See the report for the complete measurement boundaries.

Plots are generated automatically by the final CMake step:

- `out/build/benchmark/single-allocation-soa-plots/timings.png` and `relative-performance.png` (other operations)
- `out/build/benchmark/single-allocation-soa-plots/reserve-comparison-65536.png`
- `out/build/benchmark/single-allocation-soa-plots/reserve-records.csv` (nanoseconds, confidence bounds, samples)

The bar chart uses a logarithmic time axis so both means remain visible. SVG is also exported, but PNG is the convenient default. CTest's source log is `out/build/benchmark/Testing/Temporary/LastTest.log`; save it before another test run replaces it. Existing plots from previous runs are not deleted.

Reserve without correctness, including build and plotting:

```powershell
cmake --workflow --preset single-allocation-soa-reserve
```

Rerun with a custom sample count after building (two processes, saved logs and plots):

```powershell
./Scripts/run-soa-reserve-benchmark.ps1 -Samples 100
```

The script writes to `.local/benchmarks/single-allocation/reserve/`; use `-OutputDirectory` to preserve another run. For profiling, select either exact Catch2 test name, such as `SandboxCore.SingleAllocation.Timing.reserve_TArray`. To rerun via CTest or regenerate plots:

```powershell
ctest --preset single-allocation-soa-reserve
cmake --build --preset single-allocation-soa-plots
```

The plotting script uses `uv` and its declared matplotlib dependency. It also accepts saved logs with `--input` and `--output-dir`. Images are replaced atomically; if a viewer locks an image, a new filename is printed instead of losing the result.

## Correctness and optional native experiment

```powershell
cmake --preset benchmark
cmake --build --preset benchmark
ctest --preset benchmark-tests -R 'SingleAllocation.BenchmarkCorrectness' --output-on-failure
cmake --workflow --preset native-soa
```

The native workflow runs correctness and dry-run/plot validation, not useful performance measurements. The more expensive `native-soa-reserve` and `native-soa-reserve-matrix` workflows remain available only when explicitly selected. See the report before comparing their results: the original native reserve benchmark excludes destruction, whereas the matrix and Unreal comparison include it.
