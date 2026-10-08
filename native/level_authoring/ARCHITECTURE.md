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
captures the loader context without a process-wide registry. Windows file capture retains
the validated file handle through reading and lives under `runtime/lib/src/platform/windows`.

`native-level-authoring` depends on `native-s7-data` and `native-levels`, without the
interpreter or simulation. Its parsers accept a valid AST and return `std::expected`
definitions with structured diagnostics. `native-level-parser-tests` links this boundary
without s7. The `native-level-authoring-s7` library provides source/file reader entry points;
it destroys the interpreter before parsing and semantic validation.

`native-levels` owns LevelId, CampaignId, EntityId, TeamId, declarative definitions,
and validation, split into metadata, entity, camera, grid, and mission definitions. It reuses
the existing core vector type and shared rotation type through `native-core-headers`,
without linking the core runtime. Grid dimension rules live with the level definitions.
Simulation depends on it and retains all runtime compilation/state.
Team IDs and entity archetypes are enums parsed from LispB-defined spellings at input
boundaries. Declared teams include unpopulated participants and affect fighter capacity;
they cannot be replaced with the set of teams found in authored entities. Semantic validation
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
