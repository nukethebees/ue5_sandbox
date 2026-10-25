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
