# Level authoring

```text
Scheme source -> sandbox-s7 -> owned s7::Ast
                                  |
                    interpreter destroyed
                                  |
                    native-level-authoring
                      parse_level / parse_campaign
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
definitions with structured diagnostics. `native-level-parser-tests` links this boundary
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
checks references, role constraints, and numeric rules independently of AST parsing.
Catalog validators receive only successfully decoded definitions, retaining original
discovery indices and filesystem paths. Diagnostics distinguish `node_path` from
`source_path` and carry an error code and message.

LispB schemas define grammar keywords and enum spellings. Generated serialized parsers share
the small `ioj::lookup_enum` helper. See
[DSL syntax](../../LevelScripts/README.md). The writer uses a small S-expression emitter
because it emits evaluable constructors, while the AST represents their evaluated data.
Literal vectors and ID lists are quoted; collections containing constructors use `list`.
The writer preserves deterministic ordering and three-decimal rounding. Editor serialization
continues to reject par-time, unlock criteria, and mission events.
