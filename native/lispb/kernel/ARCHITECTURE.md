# Kernel DSL architecture

A kernel module declares operation semantics, operand storage, concrete type sets, public variants,
aliasing contracts, and named emission profiles. The DSL deliberately excludes arbitrary C++ bodies,
ABI spelling, and runtime evaluation so generated kernels remain explicit and reviewable.

Emission profiles select standard or Unreal output and optional isolated SIMD laboratory outputs.
They declare headers, sources, tests, includes, namespaces, and dispatch artefacts as appropriate.
Maps and sums expand the declared type sets into concrete generated operations; validation rejects
ambiguous names, unsupported combinations, and output collisions before writing files.

Generated implementations preserve per-type operations instead of hiding column work behind
variadic abstractions. Native SIMD experiments are separate profiles and do not change the portable
or Unreal contract without generated-source review.

See [README.md](README.md) for the DSL entry point.

The native SIMD profile accepts an optional `highway-source` output. It emits direct Highway
operations using the same loop, unrolling, and layout rules as the intrinsic backends. CMake
compiles that source separately for AVX2 and AVX-512; target and lane assertions prevent silent
fallback. These backends are experimental and do not change production dispatch.

Build `native-core-highway-benchmark-report` and `kernel-highway-benchmark-report` with the
`native-benchmark` configuration for focused packing and kernel comparisons. JSON results go to
`.local/benchmarks/highway/`. Compare like-named intrinsic and Highway rows using median real time
and repetition variability; unsupported Highway targets are reported as skipped.
