# Enum groups and ordering

Groups generate constexpr, nodiscard, noexcept membership predicates in the enum's namespace.
Group names are C++ identifiers; `actionable` generates `is_actionable`. Membership is declared
on enumerators, so a group exists when at least one enumerator names it. Each enumerator may
belong to several groups.

Each group also generates an `inline constexpr std::array` named
`<snake_case_enum_name>_<group>_values`, such as `leaf_kind_actionable_values`.
Arrays follow the final enum declaration order, preserve named aliases, and exclude the
declared count sentinel. They do not require enum traits. Unreal projection arrays use
the projected enum name, including its `E` prefix.

```lisp
(enum LeafKind std::uint8_t
  :enum-array true
  :ordering (actionable)
  (value TimerElapsed)
  (value EntityDestroyed :groups (actionable))
  (value LatchedTrue)
  (value EntityReachedPosition :groups (actionable))
  (value EntityKillsReached :groups (actionable))
  (projections (cpp)))
```

This assigns `EntityDestroyed`, `EntityReachedPosition`, and `EntityKillsReached` codes 0–2,
followed by `TimerElapsed` and `LatchedTrue`. The predicate is
`return value <= LeafKind::EntityKillsReached;`.

| Ordering | Behaviour |
|---|---|
| Omitted or `manual` | Preserve declaration order and all numeric assignments. |
| `(B C A)` | Stable lexicographic sort, with membership before non-membership, comparing B, then C, then A. List every used group exactly once. |
| `auto` | Start with group priorities in first-occurrence order; accept priority swaps that reduce the total number of membership runs. Keep declaration order if it has an equal or lower score. |

Automatic ordering is deterministic and best effort. Both reordering modes preserve declaration
order for identical membership patterns. A declared `:count` enumerator remains last. Reordering
assigns consecutive zero-based codes; explicit assignments are accepted only when their known
numeric values match those positions. Non-literal assignments cannot be verified and are rejected
when reordering. To keep an existing numeric ABI, use manual ordering.

Predicates merge consecutive numeric members into ranges, use equality for singletons, and use a
switch when more than four ranges remain. Sparse holes and non-members are excluded. An unsigned
range beginning at zero needs only its upper bound. Manual enums containing unresolved C++
initializers use equality comparisons. Numeric aliases must have identical memberships because
their values cannot be distinguished at runtime.

Traits, string tables, parsers, Unreal projections and conversions, and packed fields all use the
same arranged enumerators. No runtime group metadata is emitted, and ordering does not change
the requested storage type or logical width. The normal `:enum-array true` constraints still apply.
Canonical schema output retains authored order and group annotations.
