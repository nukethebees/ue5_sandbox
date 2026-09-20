# Ray/AABB slab-ordering benchmark

Status: Historical decision record; the benchmark harness is retired.

Date: 2026-09-06

## Decision

Keep the current production implementation. It already precomputes reciprocal ray direction once
per trace. Do not preselect near/far slab arrays per ray or add eight direction-sign template
specializations.

## Compiler observations

The Development Win64 build used `/Ox /Ot /arch:AVX2 /fp:fast`. MSVC emitted the current
per-axis ordering as branchless scalar `vminss`/`vmaxss`, not a branch from
`if (t1 > t2) Swap(t1, t2)`. The traversal remained scalar: early exits, closest-hit reduction,
filters, and indexed static candidates prevented packed-SIMD autovectorization.

## Historical results

The retired benchmark covered mixed hit/miss AABBs across all eight ray octants for trace and
swept segments at 1, 4, 16, 64, 128, and 2,048 AABBs. It used 100 samples, 10,000 resamples, and
a 500 ms warm-up per named benchmark.

Preselecting near/far SOA views reduced geometric-mean scalar latency by 12.8% for 4--128 AABB
cases (about 14.6% more throughput), but regressed small realistic batches:

| Case | Current | Ordered views | Change |
|---|---:|---:|---:|
| Trace, 1 AABB | 170 ns | 203 ns | +19% latency |
| Sweep, 4 AABBs | 291 ns | 366 ns | +26% latency |
| Trace, 64 AABBs | 4.64 us | 3.65 us | -21% latency |
| Trace, 2,048 AABBs | 112 us | 96 us | -14% latency |

The comparison-only contiguous-SOA case also remained scalar, so its improvement did not show an
autovectorization benefit for the production traversal. Eight sign-specialized implementations had
inconsistent results and added about 6.5 KiB of hot code versus about 1.1 KiB for the ordered
generic routine. The code-size and instruction-cache cost did not justify the measured throughput.

## Correctness and limitations

The benchmark checked equivalent results over all octants, trace/sweep mode, and existing
finite-value behavior. Production zero-direction handling, including signed zero, remained
unchanged. NaN and infinite inputs were outside the supported comparison contract under fast
floating-point semantics; changing them was not a goal.

## Historical reconstruction

The Unreal/Catch2 `SandboxCoreBenchmarks` executable and its source harness were retired with the
Low Level Test removal in `84bbbe678`. They are not runnable from HEAD. The last pre-removal
revision, `6c511b0f7`, contains the historical source at
`Plugins/SandboxCore/Tests/SandboxCoreBenchmarks/Private/ray_aabb_benchmarks.cpp` for evidence or
reconstruction when necessary; this record does not reinstate that infrastructure.
