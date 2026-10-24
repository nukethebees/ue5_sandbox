# Record settings

Keep fields, types and C++ defaults in ordinary records. Opt in to exposing their fields:

```lisp
(record Weapon :generate-settings true :generate-overrides true
  (member damage float :initializer "5.f")
  (member turn_rate float :initializer "2.f" :settings-name "turn-speed")
  (member socket_distance float :settings-exclude true))

(settings-schema Fields :target game::Settings :target-header "game/settings.h"
  (section "player.laser" "player.laser" :record Weapon)
  (section "turrets.laser" "turrets.laser" :record Weapon)
  (field "tick.rate" "clock.tick_rate"))
```

`section` expands a record into the existing settings enum, key lookup and typed visitor.
Nested value records expand recursively and must also enable settings. Semantic type references
work across modules. Underscores become hyphens in external names; `:settings-name` overrides
that name and `:settings-exclude true` omits a member. Fixed arrays and optional-valued members
are leaves, not recursively expanded sections. Explicit `field` mappings remain available for
handwritten targets and external adapters, including Unreal types. The target itself need not
be a generated record. Empty sections, duplicate paths and recursive expansion are errors.

`:generate-settings true` alone adds no fields or runtime overhead to the record. Existing
records without these options retain their output. Sparse S7 assignments still apply directly
through typed dispatch, in layer order; parsing and effective-configuration validation remain
the consumer's responsibility. No patch object or comparison with defaults is needed.

Separately, `:generate-overrides true` generates `WeaponOverrides` and an inline
`apply_overrides(Weapon&, WeaponOverrides const&)`. It requires settings generation. Every
included member becomes an absent-by-default `std::optional`. A present field replaces the
destination even when its value equals the C++ default. Applying another patch later gives it
precedence. Direct nested records use optional nested patches and must also enable override
generation. Other members replace their complete value. For an optional-valued member the outer
optional represents presence, so an engaged outer optional containing `std::nullopt` clears it.
Excluded fields are omitted from patches as well as settings dispatch.

The simulation schema uses section bindings and direct sparse assignment application. It does
not generate unused production patches. Compiled fixtures exercise the opt-in patch facility;
archetype-specific entity spawn overrides remain handwritten.
