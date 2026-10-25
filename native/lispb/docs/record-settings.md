# Record settings

Keep fields, types and defaults in ordinary records:

```lisp
(record Weapon :generate-settings true
  :generate-s7-reader true :s7-tag "weapon"
  (member damage float :initializer 5.f :nonnegative true)
  (member turn_rate float :initializer 2.f :settings-name "turn-speed")
  (member socket_distance float :initializer 0.f :settings-exclude true))
```

This generates the ordinary `Weapon` and its typed S7 reader. Configuration records contain
no presence flags or parsing state. There are no companion override structs.

Omitted fields preserve the destination; present fields replace it even when equal to
their defaults. `read_override(reader, node, path, existing)` decodes into a copy of the
existing configuration. Nested records retain omitted members; other types replace their
whole value. An omitted optional-valued member remains unchanged; `:none` clears it.
An invalid layer returns diagnostics without modifying the caller's original value.

Underscores become hyphens in external names. `:settings-name` customizes the name.
`:settings-exclude true` omits asset-derived members from readers.
Records without settings options retain their existing generated output.

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
in schema declaration order. `Field::Count` equals the number of members, including
settings-excluded members; it is zero for an empty record. Consequently, each integer
in `[0, Count)` can be cast directly to a valid field and used for iteration or array indexing:

```cpp
using Field = Weapon::Field;
constexpr auto count{static_cast<std::size_t>(Field::Count)};
for (std::size_t index{}; index < count; ++index) {
    Weapon::visit_field(weapon, static_cast<Field>(index), visitor);
}
```

These indices follow schema order and are not stable persistent identifiers across schema edits.

The object must be a mutable or const lvalue of the record type. Const objects produce
const member references. The callable must compile for every member; use `if constexpr`
when an operation applies only to particular fields. Dispatch invokes it exactly once.
`Field::Count` and other invalid IDs violate the caller contract. The generated API reserves
`Field`, `Count` and `visit_field`; it adds no object storage and preserves aggregate layout.
Record names that collide with `Field`, `visit_field`, `Self` or `Visitor` are rejected.

Visitors expose all members, including settings-excluded ones, without S7 dependencies.
Opted-in named S7 readers reuse the visitor; external names, requiredness, exclusions and
constraints remain in the reader. Non-default-constructible records still stage values
before construction, and positional readers retain their direct argument decoding.
For constraint-policy records, the visitor belongs to the underlying `NameValue` record;
the validated wrapper does not expose mutable storage.
