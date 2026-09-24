# Lispb code generation

Lispb contains the small declarative DSLs used to generate SandboxCore kernels, Slate layouts, and
Unreal material assets. Use the repository CMake generation targets rather than invoking emitters
directly.

External C++ types can carry scalar information without generating a new C++ type. For example,
put these definitions in a shared registry file:

```lisp
(type signed_byte :spelling "int8" (integer :signed true :bit-width 8))
(type real :spelling "double" (floating-point :format ieee754-binary64))
```

Include it from the project's `:types` file with `(include "common/scalars.lispb")`. Include paths
are relative to the containing file. The planner still lists these types as external, but shows
their scalar properties. Integer definitions can also supply domains for packed fields,
quantization, varints, and optionals. Size and alignment come from the selected ABI profile.
The project's [common scalars](../../lispb/schema/common_scalars.lispb) provide standard examples.

See [Semantic type graph](SEMANTIC_TYPE_GRAPH.md) for the shared resolved model consumed by code
generation and layout tooling. See [ARCHITECTURE.md](ARCHITECTURE.md) for the shared generation
pipeline and the sibling DSL documents for their language-specific contracts.
