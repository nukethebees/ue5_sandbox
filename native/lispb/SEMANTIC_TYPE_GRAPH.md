# Semantic type graph

LispB S-expressions are the canonical editable and persisted schema. `load_sources` parses them
into the validated `codegen::Manifest`; `lispb::schema::resolve_type_graph` then resolves the
declarations once into a consumer-neutral `TypeGraph`.

Normal modules expose one ordered `DeclarationSchema` sequence. The graph visits that sequence
directly, so a semantic node's declaration index is the index in its source module, including
when different declaration kinds are interleaved. Presentation-only declarations may occupy an
index without contributing a graph node. Settings and umbrella output constructs are exceptional
modules rather than semantic declaration containers.

Each node has a stable `TypeIdentity` made from its origin, module, namespace, and declared name.
References inside one resolved graph use compact `TypeId` indices; these are handles for that graph,
not persistent identifiers. Registered native types which match a declaration resolve to that
declaration. Other registered or raw C++ types become explicit external leaf nodes, retaining the
known spelling, header, and passing policy without claiming knowledge of their structure.

The graph keeps logical meaning separate from physical representation. An enum node references its
underlying type, a packed node references both its storage and the logical type of every bit field,
a record node references each member type, and a SoA node references each column type. Packed
fields, integer scalars, record members, and SoA columns may also carry explicit labeled semantic
relationships
to declared nodes. Those relationships participate in graph navigation and reference safety but do
not independently create storage. ABI and layout consumers follow physical type references to
derive sizes; they do not replace an enum or packed value with its storage type in the semantic
model. `dependencies_of` and `users_of` expose the resulting directed graph.

`lispb-schema` owns parsing-adjacent schema validation and this resolved graph. C++ code generation
and `native-layout` are peer consumers. Consumer-specific analysis results and editable layout
experiments may project the graph, but must not duplicate or discard LispB semantics. New logical
kinds such as structs, unions, arrays, and containers should extend `TypeDefinition` and resolution
rather than introduce another schema hierarchy.

`EditableSchemaDocument` is the authoring boundary above the resolved graph. It owns a mutable
validated manifest draft, source-file ownership and declaration ranges, stable declaration IDs,
and typed undoable commands. A successful command resolves a replacement `TypeGraph`; a command
which fails validation leaves both the draft and its last valid graph unchanged. The document is
the foundation for source-preserving LispB serialization and must remain in `lispb-schema` rather
than becoming planner-owned state.
