# Semantic type graph

LispB S-expressions are the canonical editable and persisted schema. `load_sources` parses them
into the validated `codegen::Manifest`; `lispb::schema::resolve_type_graph` then resolves the
declarations once into a consumer-neutral `TypeGraph`.

Normal modules expose one ordered `DeclarationHandle` sequence. The graph visits that sequence
directly, so a semantic node's declaration index is the index in its source module, including
when different declaration kinds are interleaved. Semantic nodes retain their source declaration
handles. Settings and umbrella output constructs are exceptional modules rather than semantic
declaration containers.

Each node has a stable `TypeIdentity` made from its origin, module, namespace, and declared name.
References inside one resolved graph use compact `TypeId` indices; these are handles for that graph,
not persistent identifiers. Registered native types which match a declaration resolve to that
declaration. Other registered or raw C++ types become explicit external leaf nodes, retaining the
known spelling, header, and passing policy. Optional registry scalar contracts describe integer
domains or IEEE floating-point formats without changing external identity or supplying ABI layout
facts. Matching plain C++ references also receive this metadata. See the
[external scalar registry](ARCHITECTURE.md#external-scalar-registry) for syntax and include rules.

The graph keeps logical meaning separate from physical representation. An enum node references its
underlying type, an alias-emitted integer scalar references its configured C++ representation
(constants-only emission does not establish one),
and a packed node references both its storage and the logical type of every bit field,
a record node references each member type, and a SoA node references each column type. Packed
fields, integer scalars, record members, and SoA columns may also carry explicit labeled semantic
relationships
to declared nodes. Those relationships participate in graph navigation and reference safety but do
not independently create storage. ABI and layout consumers follow physical type references to
derive sizes; they do not replace an enum or packed value with its storage type in the semantic
model. `dependencies_of` and `users_of` expose the resulting directed graph.

Each `ResolvedOccurrenceId` names shared semantic type, C++ type, and `PhysicalTypeUse` columns.
Bindings, type uses and semantic children retain the same four-byte handle. A `Node*` member
navigates to canonical `Node`, while its physical form is an object pointer.
Only value containment participates in aggregate cycle validation. C++ dependency ordering uses
the same classification and emits local aggregate forward declarations for indirection.
The bounded classifier preserves original C++ spelling for lowering; unsupported declarators are
explicit rather than silently reduced to the base type. `native-layout::PhysicalFactsResolver`
combines these uses, derived declaration layouts and target-profile facts without a second schema.

Static-table columns describe rows, typed fixed-count columns and group result types. Facade columns
describe target bindings and method signatures; target dependencies do not imply by-value
storage. These semantic models do not assert an ABI layout for facade reference members.

Ordinary SoA declarations own their ordered logical members. A nested member's resolved type
identifies its child declaration, including across modules. There is no separate nested-name map
or vector declaration category. Nested value cycles are rejected before lowering. Physical columns
are flattened from those same members, while logical access preserves the nesting. Equivalent row
types and member mappings are independent of storage. Dynamic owners use their declaration name;
layout-only declarations have no C++ owner spelling but remain valid nested groups and relationship
targets.

`type_uses()` additionally records explicit schema references with their owning declaration or
module and role. This includes declaration-only constructs, function signatures,
registered function/validation dependencies, and settings. These uses support dependency
inventories without making every owner a semantic type. They do not parse embedded C++ text or
claim that a registration with no LispB references is unused throughout a C++ project.

`lispb-schema` owns parsing-adjacent schema validation and this resolved graph. C++ code generation
and `native-layout` are peer consumers. Consumer-specific analysis results and editable layout
experiments may project the graph, but must not duplicate or discard LispB semantics. New logical
kinds such as structs, unions, arrays, and containers should extend the semantic stores and resolution
rather than introduce another schema hierarchy.

`ioj::layout::Document` is the authoring boundary above the resolved graph. Named transaction
operations edit the shared declaration stores; `DocumentQuery` exposes readable typed views.
Instructions may leave an intermediate draft invalid. Commit resolves and validates the completed
transaction once, then publishes a replacement `TypeGraph`. A rejected transaction leaves the
published document and history unchanged and records diagnostics in the document's central log.

LispB retains schema validation, the reference visitor, queries, and canonical serialization.
Renames and cross-module moves repair explicit schema references; embedded C++ remains authored
text. The layout document coordinates project/schema/variant history through forward changes and
a cursor. Graph-local `TypeId` values and query views must be reacquired after publication.
