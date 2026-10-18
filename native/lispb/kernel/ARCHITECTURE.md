# Kernel DSL architecture

A kernel module declares operation semantics, operand storage, concrete type sets, public variants,
aliasing contracts, and named emission profiles. The DSL deliberately excludes arbitrary C++ bodies,
ABI spelling, and runtime evaluation so generated kernels remain explicit and reviewable.

The compiler-owned document stores modules, operations, operands, variants, emissions, and type sets
in separate columns addressed by typed IDs. Child lists occupy flat ranges, and names and paths refer
to the shared string store. Parsing resolves operand and type-set IDs once. Concrete variants share
storage combinations across scalar types and remain in the document for all emission profiles.

Emission profiles select standard or Unreal output. They declare headers, sources, tests, includes,
and namespaces. Maps expand the declared type sets into concrete generated operations; validation rejects
ambiguous names, unsupported combinations, and output collisions before writing files.

Generated implementations preserve per-type operations instead of hiding column work behind
variadic abstractions. Component maps bind each operand's columns to the same scalar expression.

See [README.md](README.md) for the DSL entry point.
