# Record settings

Keep fields, types and defaults in ordinary records:

```lisp
(record Weapon :generate-settings true :generate-overrides true
  :generate-s7-reader true :s7-tag "weapon"
  (member damage float :initializer 5.f :nonnegative true)
  (member turn_rate float :initializer 2.f :settings-name "turn-speed")
  (member socket_distance float :initializer 0.f :settings-exclude true))
```

This generates the ordinary `Weapon`, a sparse `WeaponOverrides`, typed S7 readers,
and `apply_overrides(Weapon&, WeaponOverrides const&)`. Configuration records contain
no presence flags or parsing state. The companion override type uses optional members.

Omitted fields preserve the destination; present fields replace it even when equal to
their defaults. Nested settings records use nested patches and must enable overrides.
Other types replace their whole value. An optional-valued member has two presence levels:
the outer optional means an override was supplied; an explicit `:none` clears the value.

Underscores become hyphens in external names. `:settings-name` customizes the name.
`:settings-exclude true` omits asset-derived members from both readers and patches.
Records without settings options retain their existing generated output.

Simulation, presentation and starfield settings use these typed readers. `settings-layers`
applies patches in order, validating supplied fields as they are decoded. Simulation's
handwritten root applies the generated groups, then validates cross-field constraints.
Shared configuration types live in `native/levels`; runtime state remains in simulation.
Archetype-specific entity spawn overrides remain handwritten.

The former `settings-schema` declaration and flattened path/value dispatcher have been
removed. See [S7 readers](s7-readers.md) for decoding and constraint policies.
