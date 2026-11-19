# Lispb architecture

The project manifest names each generation target, its sources and its output location.
Game schemas live in `lispb/schema/`. The compiler's own schemas live alongside their
frontends under `native/lispb/`. Start with the [project manifest](../../lispb/project.lispb)
and the [generation guide](README.md).

## Compilation

`lispb::Compiler` owns source text, syntax, diagnostics and intermediate representations
for a compilation. The selected frontend parses the source and checks its semantics.
For schemas, the type graph resolves references and records logical types and layouts.
Code generation and the layout planner share this graph. See the [compiler entry point](src/compiler.cpp)
and [semantic type graph](SEMANTIC_TYPE_GRAPH.md).

Schema, Slate and kernel targets lower into typed C++ IR. The C++ renderer resolves names and
emits the source. Generated production C++ lives alongside handwritten code in the consuming
module. The material backend produces graph data for Unreal asset generation.
See the [Slate](slate/ARCHITECTURE.md), [kernel](kernel/ARCHITECTURE.md) and
[material](material/ARCHITECTURE.md) backend guides.

## External scalar registry

A schema target's `:types` file binds existing C++ types to their headers and optional numeric
domains. Registry includes resolve relative to the including file. The compiler follows these
dependencies and rejects cycles. An encoding width describes the stored value, independently
of the C++ type's size or alignment. See the [game type registry](../../lispb/schema/types.lispb).
