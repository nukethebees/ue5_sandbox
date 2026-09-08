# Single-allocation generated SoA experiment

The comparison uses an opt-in generated owner with one aligned allocation, one size and one capacity for all 53 flattened columns. The runtime and generator mode are now production facilities in SandboxCore, with mimalloc as the Unreal default. Comparison schemas remain experimental and production fighter data is unchanged. See the [storage API and lifetime contract](../SandboxCore/single_allocation_storage.md).

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

Actual-value append is measured separately by `append_from_natural_64`, `append_from_natural_65536`, `append_from_reserved_64` and `append_from_reserved_65536`. Source construction/defaulting is outside timing. Natural cases include destination allocation and destruction; reserved cases reuse storage and include reset. Sources contain 65,536 rows, copied in 64-row batches or one whole-source operation.

`remove_indices_clustered` and `remove_indices_scattered` remove 16,384 original rows from a 65,536-row owner. Index-list construction is untimed; timing includes restoring logical size/defaulting before removal, but does not restore original row values. Both sides use ascending surviving-tail block order. TArray has no equivalent generated index-list operation, so its benchmark adapter uses the same movement-range traversal with column copies and a final per-array resize. This avoids comparing different survivor ordering.

`view_handles` measures compact handle creation/consumption, while `materialize_columns` measures conversion to the existing column-span aggregate. Each performs 4,096 calls; `construct_views` retains the existing aggregate-consuming workload. Iteration materializes spans outside the timed loop for both owners. Compact views are 16 bytes each, but the explicitly materialized aggregate is still large. These cases are also registered in the optional native Google Benchmark executable, whose vector append validates source column lengths and rejects aliasing; self-append support belongs to the single owner.

Correctness includes `BulkBenchmarkCorrectness` before timing. Historical measurements in the report predate these APIs and should not be interpreted as results for the current implementation.

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
