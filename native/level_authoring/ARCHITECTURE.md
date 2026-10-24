# Level authoring

```text
Scheme source -> sandbox-s7 -> owned s7::Ast
                                  |
                    interpreter destroyed
                                  |
                    native-level-authoring
                      parse_level / parse_campaign / parse_settings
                                  |
                          native-levels
                      semantic / catalog validation
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
definitions with structured diagnostics. Level and campaign parsers borrow the AST and take
an explicit root node, so catalog parsing never changes or copies AST storage.
`native-level-parser-tests` links this boundary
without s7. The `native-level-authoring-s7` library provides source/file reader entry points;
`DefinitionReader::read_root_file` evaluates one explicit root and its imports in one
interpreter. `catalog` accepts `:levels` and `:campaigns` filename lists and evaluates
them in the supplied order. There is no directory discovery or automatic library preloading.
`load-script` caches each successful file's final value, so repeated imports do not reevaluate
it. Paths are relative to the root file's directory. Standalone level/campaign reads remain
available to the editor and benchmark runner.

The evaluated catalog retains definition source paths and per-file evaluation failures.
It is materialized into the same flat AST, then the interpreter is destroyed before
`parse_catalog` and semantic validation. The catalog parser belongs to the runtime-free
authoring library and dispatches to the existing level/campaign parsers. Each selected source
produces a `DefinitionEntry<T>` containing its path and `std::expected<T, Diagnostics>`.
Catalog validators still consume only successful definitions.

Bindings are shared during a root load, including changes made before an evaluation error.
A new root load starts fresh; no interpreter or bindings persist between catalog refreshes.
The runtime privately roots s7's internal hooks after revoking their public bindings, keeping
global redefinition and error handling safe across garbage collections.

`native-levels` owns LevelId, CampaignId, EntityId, Team, declarative definitions,
and validation, split into metadata, entity, camera, grid, and mission definitions. It reuses
the existing core vector type and shared rotation type through `native-core-headers`,
with file/ASCII utilities supplied by core. Grid dimension rules live with the level definitions.
Simulation depends on it and retains all runtime compilation/state.
Teams and entity archetypes are enums parsed from LispB-defined spellings at input
boundaries. Participating teams are derived from initial and scheduled entities. The shared `ioj::Team`
enum is used throughout authoring and simulation; only Unreal properties use its reflected projection. Semantic validation
checks references, expression and activation cycles, and numeric rules independently of AST parsing.
Mission expressions use objective-local indices; only published outcomes can cross objective
boundaries. Runtime compilation flattens parents, rules and reverse dependency references.
Generated entity-ID counters use fixed team/archetype slots.
Catalog validators receive only successfully decoded definitions, retaining original
discovery indices and filesystem paths. Diagnostics distinguish `node_path` from
`source_path` and carry an error code and message.

LispB schemas define grammar keywords and enum spellings. Generated parsers use string comparisons
and switches without runtime lookup tables. See
[DSL syntax](../../LevelScripts/README.md). The writer uses a small S-expression emitter
because it emits evaluable constructors, while the AST represents their evaluated data.
Literal vectors and ID lists are quoted; collections containing constructors use `list`.
The writer preserves deterministic ordering and three-decimal rounding. Editor serialization
rejects par-time, unlock criteria, objective missions, and settings layers. Objective graphs
remain authored in S7; the scene editor cannot infer or rewrite them from entity roles.

Settings constructors return records, and `settings-layers` groups records in application order.
`parse_settings` flattens nested records into owned `ioj::settings::Setting` assignments with
dotted paths and scalar/numeric-vector values. It preserves order and field presence; there
is no default-filled patch mirror or interpreter state in runtime configuration. Level definitions
retain their optional settings assignments. The native simulation resolver applies them directly
to existing simulation structs; presentation consumers apply their own sections. Unknown fields
and type mismatches are configuration errors at the owning consumer. Asset-derived geometry
and Unreal resource references remain at the Unreal boundary.

AST parsers, Scheme file readers and serializers live under `s7/src/`. General authoring code
such as scene reconciliation and source sessions remains under `lib/src/`. The AST-only and
interpreter-backed libraries keep their existing dependency boundaries.

Lispb `settings-schema` declarations generate field-name enums, keyword lookup and typed member
dispatch beside the S7 readers. The readers default-construct native settings, assign supplied
fields through that dispatch and then validate domain constraints. Generated adapters stay
separate from runtime types; simulation and presentation code consume resolved settings.

Ordinary records with matching script/domain layouts (camera, grid, campaign, team result)
decode directly through generated companion readers. Level/entity/objective inputs remain
separate where decoding must precede ID assignment, layout conversion or graph construction.
This avoids teaching the generator domain-specific destination paths or reference resolution.
Generated readers mutate a local default-constructed value when available and stage members
only when construction is necessary. Presence masks distinguish omissions independently of
member values. Fixed positional readers handle named expressions, references, dependencies
and unlock criteria. Expression recursion, leaf selection and reference resolution remain explicit.

Sparse settings retain their owned assignment representation: one level may contain native,
Unreal sections consumed at different times. These consumers still use generated field bindings.
Replacing that path must preserve authored presence, layer order and separately consumed sections. The generic
settings walker contains no per-setting field lists; those are generated from the schemas.
Structural record readers and settings bindings share Lispb's keyword lookup generation; their numeric conversions
share `try_numeric_cast`, with flattened settings rejecting nonfinite values at assignment.
Starfield now reads directly into generated native records: an outer section/layer traversal
calls the same typed decoder as ordinary records, then validates effective values. Its old
field bindings and handwritten constraint chain are removed. `read_override` applies a partial
record to a copy, leaving the caller's instance untouched on failure.

`settings-schema` remains only for simulation and Unreal presentation consumers. Moving plain
presentation data into native readable records and migrating simulation's root bindings would
allow those mappings and the flattened-assignment pipeline to be removed. This is not a reason
to move asset references out of Unreal: engine resources can stay in a separate adapter. UI
widgets, including colour pickers, can edit native values without owning their configuration.
Cross-field rules such as attack-distance bands must still validate after all override layers.
