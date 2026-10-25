# Record settings

Keep fields, types and defaults in ordinary records:

```lisp
(record Weapon
  :generate-s7-reader true :s7-tag "weapon"
  (member damage float :initializer 5.f :nonnegative true)
  (member turn_rate float :initializer 2.f :s7-name "turn-speed")
  (member socket_distance float :initializer 0.f :s7-exclude true))
```

This generates the ordinary `Weapon` and its typed S7 reader. Configuration records contain
no presence flags or parsing state. There are no companion override structs.

Omitted fields preserve the destination; present fields replace it even when equal to
their defaults. `read_override(reader, node, path, existing)` decodes into a copy of the
existing configuration. Nested records retain omitted members; other types replace their
whole value. An omitted optional-valued member remains unchanged; `:none` clears it.
An invalid layer returns diagnostics without modifying the caller's original value.

Underscores become hyphens in external names. `:s7-name` customizes the name.
`:s7-exclude true` omits members from readers while retaining their C++ storage and defaults.
Settings use ordinary records and readers; there is no separate settings-record category.

Simulation, presentation and starfield settings use these typed readers. `settings-layers`
applies patches in order, validating supplied fields as they are decoded. Simulation's
handwritten root receives the resolved groups, then validates cross-field constraints.
Load base settings first and pass the result to `DefinitionReader::read_level_source` or
`read_level_file`. A decoded level holds complete effective settings, not a patch that can
be applied to an unrelated base later. Reading a level without a seed uses C++ defaults;
omitting settings altogether leaves its optional configuration absent.
Shared configuration types live in `native/levels`; runtime state remains in simulation.
Archetype-specific entity spawn overrides remain handwritten.

The former `settings-schema` declaration and flattened path/value dispatcher have been
removed. See [S7 readers](s7-readers.md) for decoding and constraint policies.

## Typed member visitors

Any ordinary record can opt into `:generate-visitor true`, independently of settings or
S7 readers. This adds a nested `Field` enum in member order and a static constexpr
`visit_field(object, field, visitor)` function. The visitor receives the field as a
template argument and the selected member by reference:

```cpp
Weapon::visit_field(weapon, Weapon::Field::damage,
                    []<auto field>(auto& member) {
    if constexpr (field == Weapon::Field::damage) {
        member = 10.f;
    }
});
```

`Field` is guaranteed to be zero-based and contiguous, with one enumerator per member
in schema declaration order. `ioj::enum_count<Field>()` returns the number of members,
including S7-excluded members; it is zero for an empty record. There is no count sentinel.
Each integer in `[0, count)` can be cast directly to a valid field for iteration or array indexing:

```cpp
using Field = Weapon::Field;
constexpr auto count{ioj::enum_count<Field>()};
for (std::size_t index{}; index < count; ++index) {
    Weapon::visit_field(weapon, static_cast<Field>(index), visitor);
}
```

These indices follow schema order and are not stable persistent identifiers across schema edits.

The object must be a mutable or const lvalue of the record type. Const objects produce
const member references. The callable must compile for every member; use `if constexpr`
when an operation applies only to particular fields. Dispatch invokes it exactly once.
Invalid IDs violate the caller contract. The generated API reserves
`Field`, `visit_field`, `invoke_field` and `get_field`; it adds no object
storage and preserves aggregate layout. Record names must not collide with these generated
functions, `Field`, or the template parameter names `Selected`, `Self` and `Visitor`.

`get_field<Field::damage>(weapon)` selects a member at compile time, preserving its reference
and constness. Invalid fields are rejected by template constraints. `ioj::to_string(Field::damage)`
returns the C++ member name as a static `string_view`, independent of S7 aliases, through the
ordinary packed enum string metadata. Runtime
visitation retains a switch with unbraced return cases. Each case calls `invoke_field<F>`, which
forwards the callable, discards its return value and obtains the member through `get_field<F>`.

These APIs also support [structural diffing](../../core/docs/structural-diff.md) without S7
dependencies or a second paired visitor.

Visitors expose all members, including S7-excluded ones, without S7 dependencies.
Opted-in named S7 readers reuse the visitor; external names, requiredness, exclusions and
constraints remain in the reader. Non-default-constructible records still stage values
before construction, and positional readers retain their direct argument decoding.
For constraint-policy records, the visitor belongs to the underlying `NameValue` record;
the validated wrapper does not expose mutable storage.
