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
