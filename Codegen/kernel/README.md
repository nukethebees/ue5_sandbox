# SandboxCore kernel DSL

`kernelc` generates concrete array-kernel overloads from deliberately narrow S-expression
declarations. The language describes operation semantics, operand storage, concrete types, public
variants, aliasing contracts, and named C++ emission profiles. It does not evaluate Lisp or accept
arbitrary C++ bodies or ABI spellings.

## Grammar

```text
document      := kernel_module+ EOF
kernel_module := "(" "kernel-module" identifier module_item+ ")"
module_item   := emit | type_set | map
emit          := "(" "emit" ("unreal" | "standard") emit_item+ ")"
emit_item     := header | source | tests | header_include | namespace | export
header        := "(" "header" quoted_path ")"
source        := "(" "source" quoted_path ")"
tests         := "(" "tests" quoted_path ")"
header_include := "(" "header-include" quoted_path ")"
namespace     := "(" "namespace" qualified_identifier ")"
export        := "(" "export" identifier ")"
type_set      := "(" "type-set" identifier concrete_type+ ")"
concrete_type := "int32" | "uint32" | "float" | "double"
map           := "(" "map" identifier map_item+ ")"
map_item      := types | operand | output | expression | variants | aliasing
types         := "(" "types" identifier ")"
operand       := "(" "operand" identifier storage ")"
storage       := "array" | "scalar" | "(" ("array" | "scalar")+ ")"
output        := "(" "output" identifier ")"
expression    := "(" "expression" expr ")"
expr          := identifier | decimal | constant
               | "(" ("+" | "-" | "*" | "/") expr expr ")"
decimal       := signed decimal integer, fraction, or base-10 exponent
constant      := "(" "constant" ("nan" | "infinity" | "negative-infinity") ")"
variants      := "(" "variants" variant+ ")"
variant       := "(" "out-of-place" identifier ")"
               | "(" "in-place" operand_name identifier ")"
aliasing      := "(" "aliasing" ("output-disjoint" | "pairwise-disjoint") ")"
```

The default aliasing policy is `output-disjoint`: a separate output may not overlap an input, and
an in-place target may not overlap another array operand. Read-only inputs may alias each other.
Every array in one generated overload has the same length. Empty views are valid.

Storage choices form a Cartesian product. An all-scalar out-of-place combination is discarded, an
in-place target is forced to array storage, and declaration order determines C++ argument order.
No commutative or associative rewriting is performed.

Finite decimal literals are validated against every concrete type used by the operation and are
rendered with an explicit cast to that type. Non-finite values use the explicit `constant` form and
require a type-set containing only `float` and/or `double`; bare `nan` and `inf` are ordinary operand
identifiers.

The `unreal` profile emits `TArrayView`/`TConstArrayView`, Unreal integer names, checks, owning
`TArray` conveniences, and `RESTRICT` raw kernels. The `standard` profile emits `std::span`, maps
`int32` and `uint32` to the corresponding `<cstdint>` types, and has no Unreal dependencies. A
standard emission may also request a generated GoogleTest source. Each generated overload is
exercised at empty, scalar, SIMD-boundary, and larger lengths against a scalar reference. Profile
selection is fixed in the generator rather than configurable through arbitrary C++ strings.

## Commands

```text
kernelc --manifest <path> --profile <unreal|standard> [--output-root <directory>] [--check]
cmake --build --preset codegen --target generate-kernel-code
cmake --build --preset codegen --target check-generated-kernel-code
```
