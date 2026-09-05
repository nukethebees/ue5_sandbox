# SandboxCore kernel DSL

`kernelc` generates concrete array-kernel overloads from deliberately narrow S-expression
declarations. The language describes operation semantics, operand storage, concrete types, public
variants, and aliasing contracts. It does not evaluate Lisp or accept arbitrary C++ bodies.

## Grammar

```text
document      := kernel_module+ EOF
kernel_module := "(" "kernel-module" identifier module_item+ ")"
module_item   := header | source | header_include | namespace | export | type_set | map
header        := "(" "header" quoted_path ")"
source        := "(" "source" quoted_path ")"
header_include := "(" "header-include" quoted_path ")"
namespace     := "(" "namespace" qualified_identifier ")"
export        := "(" "export" identifier ")"
type_set      := "(" "type-set" identifier concrete_type+ ")"
concrete_type := "int32" | "float" | "double"
map           := "(" "map" identifier map_item+ ")"
map_item      := types | operand | output | expression | variants | aliasing
types         := "(" "types" identifier ")"
operand       := "(" "operand" identifier storage ")"
storage       := "array" | "scalar" | "(" ("array" | "scalar")+ ")"
output        := "(" "output" identifier ")"
expression    := "(" "expression" expr ")"
expr          := identifier | number | "(" ("+" | "-" | "*" | "/") expr expr ")"
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

## Commands

```text
kernelc --manifest <path> [--output-root <directory>] [--check]
cmake --build --preset codegen --target generate-kernel-code
cmake --build --preset codegen --target check-generated-kernel-code
```
