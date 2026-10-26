# Lispb code generation

Lispb generates native C++ schemas and SoA containers, SandboxCore kernels, Slate layouts, and
Unreal material assets. Use the repository CMake generation targets. Generated headers and sources
are checked in beside handwritten code in their normal public include and source directories.

Records, unions, enums and scalar encodings are logical declarations. Add `(projections (cpp))`
to request an owning C++ type. Native records use ordinary C++ members; packed records and unions
use one unsigned integer and compile-time bit offsets:

```lisp
(integer-scalar EntityIndex :bit-width 24)
(integer-scalar EntityType :bit-width 8)
(record EntityId :layout packed
  (member index @EntityIndex)
  (member entity_type @EntityType)
  (projections (cpp)))
```

`EntityId` occupies four bytes. Its C++ API has `storage_type`, `raw_value()`, `from_raw()`,
`index()`, `set_index()`, and corresponding accessors for `entity_type`. Nested packed accessors
use the full path, such as `payload_kill_entity_target_index()`. There are no packed view proxies.
`(cpp :storage std::uint64_t)` requests wider owning storage without changing the encoded width
used when the type is embedded. Logical records without a projection can also compose into packed
records. `:bits` reserves a larger enclosing extent; it cannot truncate a child's encoding.

An unsigned `integer-scalar` infers its full domain from `:bit-width`; `:signed true` selects a
signed domain. Optional `:minimum` and `:maximum` narrow the domain. A projected integer scalar is
an explicit C++ wrapper with `value_type`, `value()`, `set_value()` and checked typed construction.
`from_raw()` preserves all storage bits without validation; `is_valid()` checks the logical domain.

Projected scalars and packed owners support `std::format` and `std::hash` automatically. Scalars
format and hash their decoded value; packed records and unions use their raw integer, matching
their equality semantics. Standard integer format specifications, including hexadecimal, apply.

Unions contain references, for example `(union Target @EntityIndex @LocationIndex)`. A record
member binds a discriminator with `(member payload @Payload :tag opcode)`, where `opcode` is a
sibling enum member. Union alternatives infer enum names from their type names, or use
`(case KillEntity @AttackPayload)` for explicit mapping. Packed factories such as
`Instruction::make_payload_kill_entity(target)` set the tag and payload together. Alternative
getters and setters assert the active tag in debug builds; setters do not change it.

An ordinary `struct` declares ordered SoA columns. Each dynamic owner uses one allocation, one row
count, and one capacity. Nested members reference another declared SoA through their type:

```lisp
(struct Points :equivalent-type Point
  (member xs array float :equivalent-member x)
  (member ys array float :equivalent-member y))
(struct Rows
  (member ids array std::uint32_t)
  (member points nested Points))
```

Owners use the default PMR resource, or accept a `std::pmr::memory_resource&` at construction.
The resource must outlive the owner. Use `get_view()` or `get_const_view()` for compact borrows,
and call `.resolve()` on a view to materialize its column pointers once before a hot loop.
Both view representations expose typed spans; neither owns or resizes columns. `:layout-only true`
declares a reusable nested column group without a standalone owner.

External C++ types can carry scalar information without generating a new C++ type. For example,
put these definitions in a shared registry file:

```lisp
(type signed_byte :spelling "std::int8_t" :header "cstdint" (integer :signed true :bit-width 8))
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
  (value Active :value "1")
  (projections (cpp)))
```

Available helpers are `to-string`, `try-parse`, `to-display-string`, `to-serialized-string`,
`try-parse-serialized`, `traits` (`ioj::EnumTraits<Enum>::values`), and `formatter`.
The [formatter helper](docs/enum-formatting.md) requires `to-string` and supports selecting
serialized/display representations when those helpers are present. Omit the list
or use `:helpers ()` for a bare enum. Display formatting falls back
to the enumerator name. Serialized helpers require `:serialized-name` on every non-count value.
Unknown or duplicate helpers are errors. Existing native schemas must explicitly select the
helpers they need; `:native-api true` no longer selects them automatically. Unreal enums use
the existing `:conversions` list instead.

Compiler keyword enums live in `schema/`, with dialect-specific enums in `slate/schema/`,
`kernel/schema/`, and `material/schema/`. Use their generated serialized-name lookups for closed
keyword sets. Grammar restrictions, such as accepting only class/struct facade friends, remain
in the parser. `generate-code` and `check-generated-code` include the `compiler-models` project
group, which covers all these enums. Regenerate with the existing compiler before rebuilding
consumers of new generated declarations.

`SchemaProperty` owns the property spellings used by the handwritten schema parser. Property
lookups and per-form allowlists use its generated enum values; `Fields` shares the same validation
and diagnostics with string-based DSL consumers. Allowed nested declarations remain grammar rules.

Use `:enum-array true` independently on native or Unreal enums to generate `ioj::enum_count<Enum>()` for
`ioj::EnumArray<Enum, T>` and `ioj::FixedEnumArray<Enum>`. LispB requires known codes in declaration
order from zero through count minus one; gaps, aliases, negative codes, hidden values, and count
sentinels are rejected. Explicit numeric initializers are allowed. Other enums can remain sparse
and do not expose the array count API. Unreal projections select their count helper separately
with `:enum-array true` inside `unreal-projection`; it is not inherited from the native enum.

See [Semantic type graph](SEMANTIC_TYPE_GRAPH.md) for the shared resolved model consumed by code
generation and layout tooling. See [ARCHITECTURE.md](ARCHITECTURE.md) for the shared generation
pipeline and the sibling DSL documents for their language-specific contracts.

Ordinary records can also supply settings field metadata and opt-in typed member visitors. See
[Record settings](docs/record-settings.md) for section bindings, exclusions and patch semantics.

Records can also opt into [typed S7 readers](docs/s7-readers.md) for AST deserialization.
