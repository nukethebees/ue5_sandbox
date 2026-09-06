# Ray/AABB slab-ordering benchmark

Date: 2026-09-06

## Decision

Keep the current production implementation. It already precomputes reciprocal ray direction once
per trace. Do not preselect near/far slab arrays per ray or add eight direction-sign template
specializations.

## Compiler output

The Development Win64 build uses `/Ox /Ot /arch:AVX2 /fp:fast`. MSVC emits the current per-axis
ordering as branchless scalar `vminss`/`vmaxss` operations, rather than a branch from
`if (t1 > t2) Swap(t1, t2)`. The candidate traversal loop is scalar: its early exits, closest-hit
reduction, filters, and indexed static candidates prevent autovectorization.

## Results

The focused benchmark tests mixed hit/miss AABBs across all eight ray octants, for trace and swept
segments, at 1, 4, 16, 64, 128, and 2,048 AABBs. It used 100 samples, 10,000 resamples, and a
500 ms warm-up for each named benchmark.

Preselecting near/far SOA views reduced the geometric-mean scalar latency by 12.8% for the
4--128-AABB cases (about 14.6% more throughput), but it regressed small realistic batches:

| Case | Current | Ordered views | Change |
|---|---:|---:|---:|
| Trace, 1 AABB | 170 ns | 203 ns | +19% latency |
| Sweep, 4 AABBs | 291 ns | 366 ns | +26% latency |
| Trace, 64 AABBs | 4.64 us | 3.65 us | -21% latency |
| Trace, 2,048 AABBs | 112 us | 96 us | -14% latency |

The comparison-only contiguous-SOA benchmark also remained scalar; it did not produce packed AVX
instructions. Its improvements therefore do not establish an autovectorization benefit for the
production traversal.

Eight sign-specialized implementations gave inconsistent results and add roughly 6.5 KiB of hot
code, versus about 1.1 KiB for the ordered generic routine. That code-size and instruction-cache
cost is not justified by the measured throughput.

## Correctness and maintenance

The benchmark checks equivalence of the variants over all octants, trace/sweep mode, and the
existing finite-value behaviour. The production code's explicit zero-direction handling, including
signed zero, remains unchanged. NaN and infinite inputs are outside the supported comparison
contract under fast floating-point semantics; changing their behaviour was not a goal.

The benchmark source is
[`ray_aabb_benchmarks.cpp`](../Plugins/SandboxCore/Tests/SandboxCoreBenchmarks/Private/ray_aabb_benchmarks.cpp).
It can be run with:

```powershell
& Binaries/Win64/SandboxCoreBenchmarks/SandboxCoreBenchmarks.exe 'SandboxCore.RayAABB.*' --benchmark-samples 100 --benchmark-resamples 10000 --benchmark-warmup-time 500 --colour-mode none
```

`--benchmark-warmup-time 500` is 500 milliseconds, not seconds.
