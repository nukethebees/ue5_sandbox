# Semantic type graph

LispB S-expressions are the canonical editable and persisted schema. `load_sources` parses them
into the validated `codegen::Manifest`; `lispb::schema::resolve_type_graph` then resolves the
declarations once into a consumer-neutral `TypeGraph`.

Normal modules expose one ordered `DeclarationSchema` sequence. The graph visits that sequence
directly, so a semantic node's declaration index is the index in its source module, including
when different declaration kinds are interleaved. A declaration can own multiple semantic types:
homogeneous layouts retain declaration identity and produce one `HomogeneousStorageType` per
value-type specialization. These nodes carry `owning_declaration`; `types_for_declaration` includes
both ordinary single-type declarations and generated families. The generic view and equivalent-type
trait remain template descriptions in the authoritative layout schema, without a fabricated
concrete TypeId or layout. Settings and umbrella output constructs are exceptional
modules rather than semantic declaration containers.

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

Each `ResolvedTypeRef` also retains the shared `PhysicalTypeUse` classification of that particular
use. A `Node*` member navigates to canonical `Node`, while its physical form is an object pointer.
Only value containment participates in aggregate cycle validation. C++ dependency ordering uses
the same classification and emits local aggregate forward declarations for indirection.
The bounded classifier preserves original C++ spelling for lowering; unsupported declarators are
explicit rather than silently reduced to the base type. `native-layout::PhysicalFactsResolver`
combines these uses, derived declaration layouts and target-profile facts without a second schema.

`StaticTableType` describes rows, typed fixed-count columns and group result types. `FacadeType`
describes its target binding and method signatures; target dependencies do not imply by-value
storage. Homogeneous storage nodes resolve element, equivalent and input types. These semantic
models do not assert an ABI layout for Unreal containers or facade reference members.

`type_uses()` additionally records explicit schema references with their owning declaration or
module and role. This includes declaration-only constructs, function signatures, allocators,
registered function/validation dependencies, and settings. These uses support dependency
inventories without making every owner a semantic type. They do not parse embedded C++ text or
claim that a registration with no LispB references is unused throughout a C++ project.

`lispb-schema` owns parsing-adjacent schema validation and this resolved graph. C++ code generation
and `native-layout` are peer consumers. Consumer-specific analysis results and editable layout
experiments may project the graph, but must not duplicate or discard LispB semantics. New logical
kinds such as structs, unions, arrays, and containers should extend `TypeDefinition` and resolution
rather than introduce another schema hierarchy.

`EditableSchemaDocument` is the authoring boundary above the resolved graph. It owns a mutable
validated manifest draft, source-file ownership and declaration ranges, document-local declaration IDs,
and typed undoable commands. A successful command resolves a replacement `TypeGraph`; a command
which fails validation leaves both the draft and its last valid graph unchanged. The document is
the foundation for source-preserving LispB serialization and must remain in `lispb-schema` rather
than becoming planner-owned state.
Renames and cross-module moves repair explicit declaration references through the shared schema
reference visitor, including moves within one C++ namespace and outgoing module-local bindings.
Before publication, these operations verify that declared references and registered aliases retain
their semantic targets. Ambiguous repairs are rejected transactionally. Replacement checks new
references against the previous graph as well as existing users before removing owned types.
Deletion checks include declaration-only users and generated storage types.
Registry-bound types retain the existing restriction on renaming/moving across namespaces until
source-aware registry editing is available; references from module configuration also prevent
those operations rather than silently becoming external types.
