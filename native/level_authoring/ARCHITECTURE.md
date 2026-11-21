# Level authoring

```text
Scheme source -> sandbox-s7 -> owned s7::Ast
                                  |
                    native-level-authoring
                      parse_level / parse_campaign / parse_settings
                                  |
                          native-levels
                      semantic validation
                                  |
                       simulation / Unreal
```

`native-s7-data` owns flat nodes, child indices, text bytes, and separate contiguous arrays
for integers, ratios, reals, and booleans. Each node contains only an enum kind and a 32-bit
offset/count into its corresponding storage. There is no union or inline payload.
Materialization rejects unsupported values, improper lists, cycles, and resource-limit
violations. No runtime handles survive in the AST. Shared acyclic sublists are copied.
The data library lives in `native/s7/data`, the interpreter and conversion in
`native/s7/runtime`, and the vendored implementation in `native/s7/third_party`.
Each interpreter owns its protected evaluation/error procedures; its private loader closure
captures the loader context without a process-wide registry. Public evaluation exposes only
owned text or AST data; s7 handles and ownership helpers are private implementation details. Windows file capture retains
the validated file handle through reading and lives under `runtime/lib/src/platform/windows`.

`native-level-authoring` depends on `native-s7-data` and `native-levels`, without the
interpreter or simulation. Its parsers accept a valid AST and return `std::expected`
definitions with structured diagnostics. The standalone parsers operate on owned ASTs and are tested without s7.

A catalog exports ordered lists of relative level and campaign paths. One interpreter loads them all.
The catalog and each entry get independent import budgets, including nested imports. C++ decodes each exported
definition individually. Entries retain their source path and diagnostics so a broken file
does not prevent valid siblings from loading. The level selector displays the bytes captured
when the level was imported.

The runtime privately roots s7's internal hooks after revoking their public bindings, keeping
global redefinition and error handling safe across garbage collections.

`native-levels` owns LevelId, CampaignId, EntityId, Team, declarative definitions,
and validation, split into metadata, entity, camera, grid, and logic definitions. It reuses
the existing core vector type and shared rotation type through `native-core-headers`,
with file/ASCII utilities supplied by core. Grid dimension rules live with the level definitions.
Simulation depends on it and retains all runtime compilation/state.
Teams and entity archetypes are enums parsed from LispB-defined spellings at input
boundaries. Participating teams are derived from initial and scheduled entities. The shared `ioj::Team`
enum is used throughout authoring and simulation; only Unreal properties use its reflected projection. Semantic validation
checks references, expression cycles, and numeric rules independently of AST parsing.
The `(logic ...)` authoring scope allocates stable expression, latch and timer handles.
Lexical references share nodes. The scope emits a flat graph before AST materialization;
runtime compilation resolves declaration references and orders combinational dependencies.
Authored latch feedback cycles are currently rejected.
Generated entity-ID counters use fixed team/archetype slots.
Catalog loading resolves unlock dependencies and campaign references in place. Broken entries retain their file paths and diagnostics.

LispB schemas define grammar keywords and enum spellings. Generated parsers use string comparisons
and switches without runtime lookup tables. See
[DSL syntax](../../LevelScripts/README.md). The writer uses a small S-expression emitter
because it emits evaluable constructors, while the AST represents their evaluated data.
Literal vectors and ID lists are quoted; collections containing constructors use `list`.
LispB generates writers from the same record metadata as readers. Full and baseline-difference
serialization preserve floating-point precision and use the existing S-expression IR/renderer.
The editor writes scene fields and preserves explicit settings overrides. Mission logic lives in the authored module.

The `s7_constructors` module derives ordinary Scheme constructors from record tags, including
positional forms, plus `foo?` and `foo-ref`. Common Scheme helpers provide tag
checks, property access and generic `copy-with` updates. Tags and generated function names
must be unique. Catalog and logic operators retain their specialized bindings.

Settings constructors return records; `settings-layers` groups records in application order.
`parse_settings` decodes them into a copy of a supplied `SettingsValues` configuration.
Level roots explicitly compose shared settings with `import-module` and `settings-layers`.
Level decoding always starts from C++ defaults; launchers cannot inject different baselines.
Simulation, presentation and starfield use the same typed readers.
Unknown fields, duplicate properties, incompatible values and field constraints are checked
during decoding. Simulation copies the effective groups to its handwritten root,
then validates the effective configuration's domain constraints.

Runtime launch state carries a validated `LevelDefinition`. Catalog selection, editor
preparation and direct file loading all evaluate the same authored root and hand the
definition to the simulation compiler. Unreal resource preparation applies those resolved
settings to asset-backed configuration without decoding another source or inheriting a
runtime-map settings file.

Shared static configuration records live under `native/levels`; runtime state remains under
simulation. Unreal retains asset references and colour controls. Asset-derived members are
excluded from authored readers. The old `settings-schema` compiler family, generated
override structs, flattened path/value assignments and simulation dispatcher are removed.
`USpaceGameLevelConfig::presentation` owns the native numeric/behavioural presentation values;
resource structs no longer contain copies of its component records. Renderers and HUDs consume
the relevant native values alongside Unreal asset references. Debug/resource editor controls
remain Unreal-owned.

AST readers, Scheme file readers and serializers live under `s7/src/`. The AST-only and
interpreter-backed libraries retain their dependency boundaries.

Ordinary records with matching script/domain layouts (camera, grid and campaign)
decode directly through generated companion readers. Level/entity inputs remain
separate where decoding must precede ID assignment, layout conversion or graph construction.
Generated readers mutate a local default-constructed value when available and stage members
only when construction is necessary. Presence masks distinguish omissions independently of
member values. Fixed positional readers handle unlock criteria. Logic graph decoding,
leaf selection and reference resolution remain explicit.
Record traversal and nested decoding append to the same diagnostic accumulator. Per-record
presence masks detect duplicates within a layer; the combined mask checks required fields.

`read_override` applies a partial record to a copy, leaving the caller's instance untouched
on failure. Each supplied layer is validated; later values cannot hide malformed earlier values.
The attack-distance band is one constrained setting. Overrides replace the whole triple and
cannot mutate individual ratios. Tick settings contain only rate and time scale; accumulation
remains in the runtime clock.
