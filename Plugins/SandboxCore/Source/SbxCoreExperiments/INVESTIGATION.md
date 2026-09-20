# Single-allocation SoA investigation archive

**Status: RETIRED / HISTORICAL.** The Unreal/Catch2 harness used for the 8–9 September 2026 allocator and column-spacing investigation was removed with the Unreal Low Level Tests. Its test names, presets, scripts, and commands are not available from `HEAD`.

The experimental module remains intentionally unchanged. This record preserves the useful findings and measurement context; it does not establish a production simulation speedup or endorse the experimental representation for production use.

## Scope and historical outcome

The investigation compared a 53-column TArray-backed owner with a single-allocation owner generated from the same experimental schema. It covered reserve, append, growth, reset, removal, iteration, and view construction. Historical `Single` means the FMemory-backed owner; `SingleMimalloc` means the direct-mimalloc owner. Those measurements are not interchangeable.

The decisive historical same-mimalloc reserve-and-destruction sweep used 65,536 rows per owner, separately launched Unreal processes, and 100 Catch2 samples per cell. The single-allocation layout was 10.6–13.9 times faster than the 53-column mimalloc TArray layout across the owner sweep. This implicated allocation/free behavior and the number of retained allocations, but did not isolate every allocator detail or model populated gameplay workloads.

The fixed-CPU spacing experiment found that a 192-byte inter-column gap improved one wide-iteration workload from 6.889 ms to 1.409 ms (4.89x) with 9,984 additional bytes. Narrow iteration changed by approximately 4%. These were separate machine-specific sessions, not a general cache-layout guarantee.

## Preserved evidence

- [Unreal same-mimalloc measurements](results/unreal-mimalloc-layouts.csv) and [plot](results/unreal-mimalloc-layouts.png): 22 reserve cases with means, confidence bounds, iterations, samples, and owner counts.
- [Native reserve matrix](results/native-reserve-matrix.csv) and [plot](results/native-reserve-matrix.png): eight allocator/owner configurations across eleven owner counts.
- [Earlier Unreal owner-isolated matrix](results/unreal-owner-isolated.csv): six configurations from the preceding methodology.
- [Initial manual timings](results/initial-manual-timings.csv) and [allocation diagnostics](results/initial-allocation-diagnostics.csv): early evidence retained for comparison, not for direct combination with the later Catch2 results.
- [Fixed-CPU spacing data](results/fixed-cpu-spacing-2026-09-09/timings.csv): raw rows for the spacing experiment.

These are committed, machine-specific artifacts. Their timing boundaries, engine/allocator configuration, CPU affinity, available memory, and concurrent load must be preserved before comparing them with another run.

## Methodology and caveats

The original matrix launched each Unreal case separately to avoid carrying allocator state between owner counts and implementations. The same-mimalloc comparison measured reserve plus batch destruction; it did not write every reserved row. Values therefore describe allocator reuse throughput under that workload, not physical-memory provisioning or a populated-frame cost.

The earlier FMemory single-block results were substantially slower and tracked raw allocation behavior. Switching to direct mimalloc changed both the allocation path and allocator configuration, so the later result strongly implicates the Unreal allocation/free path without quantifying a single cause such as memory poisoning. Single-allocation growth also temporarily owns old and new blocks, whereas individual TArrays may reallocate in place.

The native standard-library experiment used Google Benchmark with different compiler, allocator, and container implementations. It was valuable for representation-level checks and repeatable experiments, but its timings are not Unreal allocator measurements. Reserve-only results exclude different work from lifecycle and populated-operation measurements; do not compare them as though they have the same boundary.

Iteration, default construction, removal, and growth were not uniformly faster in the historical data. The practical conclusion is to validate a representative populated workload before making a production decision, rather than extrapolating from empty reserve or a single spacing result.

## Current validation and performance work

Current correctness coverage lives in the native SoA GTests, including the standard and mimalloc variants. Current repeatable performance work uses native Google Benchmark:

```powershell
cmake --workflow --preset native-soa
# Timed and explicitly opt-in:
cmake --workflow --preset native-soa-reserve-matrix
```

The first workflow includes correctness and dry-run benchmark/plot validation; its dry-run output is not a performance result. The timed matrix is intentionally separate so it can obtain the repository benchmark resource without mixing timing work into ordinary validation.

## Historical reconstruction

The complete retired Unreal harness, custom allocator variants, scripts, and captured measurement setup are retained at commit `0cf0a26c` (`Complete isolated SoA allocator comparisons with mimalloc`). Use a separate worktree at that revision if reconstruction is genuinely required. Its old commands and test names are historical only; do not apply them to the current tree.

For the early prototype and methodology changes, the relevant historical commits are `f54bfa18`, `73d36ab3`, `b8cca1d5`, and `042e0fa3`. Reconstruct the matching revision before interpreting its logs, scripts, compiler settings, or generated outputs.
