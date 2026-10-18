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

Native enums emit only the helpers selected with `:helpers`:

```lisp
(enum State std::uint8_t
  :native-api true
  :helpers (to-string traits)
  (value Idle :value "0")
  (value Active :value "1"))
```

Available helpers are `to-string`, `try-parse`, `to-display-string`, `to-serialized-string`,
`try-parse-serialized`, and `traits` (`ioj::EnumTraits<Enum>::values`). Omit the list
or use `:helpers ()` for a bare enum. Each helper is independent; display formatting falls back
to the enumerator name. Serialized helpers require `:serialized-name` on every non-count value.
Unknown or duplicate helpers are errors. Existing native schemas must explicitly select the
helpers they need; `:native-api true` no longer selects them automatically. Unreal enums use
the existing `:conversions` list instead.

Use `:enum-array true` independently on native or Unreal enums to generate `ioj::enum_count<Enum>()` for
`ioj::EnumArray<Enum, T>` and `ioj::FixedEnumArray<Enum>`. LispB requires known codes in declaration
order from zero through count minus one; gaps, aliases, negative codes, hidden values, and count
sentinels are rejected. Explicit numeric initializers are allowed. Other enums can remain sparse
and do not expose the array count API. Unreal projections select their count helper separately
with `:enum-array true` inside `unreal-projection`; it is not inherited from the native enum.

See [Semantic type graph](SEMANTIC_TYPE_GRAPH.md) for the shared resolved model consumed by code
generation and layout tooling. See [ARCHITECTURE.md](ARCHITECTURE.md) for the shared generation
pipeline and the sibling DSL documents for their language-specific contracts.
