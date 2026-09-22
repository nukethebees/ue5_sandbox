# Memory layout planner architecture

## Structure

The headless planner library lives in `native/layout/lib/`; its GoogleTest suite is in
`native/layout/tests/`. The optional SDL3 and Dear ImGui frontend lives in
`tools/layout_planner/app/`, with GUI panels in `app/gui/` and SDL-specific headers in
`app/platform/`.

```text
native/lispb            parser, validated schema, and shared semantic type graph
native/layout/lib       headless layout analysis, schema loading, and variants
native/layout/cli       compiler/configuration target-profile probe
native/layout/tests     GoogleTest coverage of the library
tools/layout_planner/app SDL3/ImGui bootstrap, event loop, and presentation
```

## Dependencies

`native-layout-tests` depends on `native-layout` and GoogleTest. `layout-planner` depends on
`native-layout`, SDL3, and Dear ImGui. The library has no SDL3, Dear ImGui, graphics, windowing,
or application-header dependency.

The normal native configuration builds and tests `native-layout` while the optional planner GUI is
disabled, which prevents UI dependencies from leaking into the library.

## State and data flow

LispB S-expressions remain the canonical source. The existing parser loads and validates a
`codegen::Manifest`, then `lispb-schema` resolves it into the shared semantic `TypeGraph`. C++
codegen and the planner are peer consumers of that graph; the planner does not maintain a second
schema catalog. See the LispB [semantic graph boundary](../../native/lispb/SEMANTIC_TYPE_GRAPH.md).

`EditableSchemaDocument` owns the source-aware mutable declaration draft and resolves a replacement
graph after every accepted semantic command. It provides stable declaration IDs, validation
rollback, undo/redo, source preview, and validated source replacement. `LayoutWorkspace` owns a
snapshot of the resolved graph, the selected aggregate analysis scale, and session-only physical
variants; graph replacement remaps variant overrides through stable type identities. `Analyzer` combines
a selected semantic type, variant overrides, and an ABI profile to produce factual packed-value or
SoA analysis results. It follows enum-underlying and packed-storage references only while deriving
physical facts, preserving the logical nodes and their dependency edges. The GUI owns selections,
dock layout, cached presentation results, and drawing; it does not own schema semantics or calculate
layouts.

The Graph view renders only `TypeGraph::dependencies_of` edges and derives labels from the resolved
definition; it owns no relationship records. Edge-label hover/click state is transient presentation
state, and navigation writes the same selected `TypeId` used by node clicks and the other views.
Search derives a fresh ordered `TypeId` match set from the current graph each frame, so it stores no
parallel topology or stale match index; Enter and **Next match** reuse selection and focus state.
Selected-neighborhood styling likewise derives direct dependencies and users from the current graph
each frame. Outgoing, incoming, bidirectional, and unrelated presentation roles are transient; they
do not duplicate relationship state or constrain cyclic/general graphs.
Manual graph positions are presentation-only world coordinates keyed by `TypeIdentity`. Graph
replacement therefore retains positions for surviving identities and gives new nodes the ordinary
column layout; adopting another project clears the map. Dragging does not create semantic commands,
dirty LispB, or alter dependency topology. Fit/focus consume the effective manual/automatic
positions, and **Automatic layout** explicitly clears the manual map.
The existing ImGui settings handler persists manual positions by normalized project path plus a
percent-escaped complete `TypeIdentity` and finite bounded world coordinates. Parsing rejects bad
escapes, origins, arity, numbers, and extreme coordinates. The active map restores only identities
present in the adopted graph; stale rows cannot create nodes or semantic state. Drag/reset marks the
ordinary ImGui settings dirty, so no separate planner project format is introduced.

## Target profiles

`AbiProfile` is an analysis input, not semantic schema. The built-in profile uses `sizeof` and
`alignof` in `native-layout` compiled by the current CMake compiler/configuration. The dedicated
Target Profile dock panel owns the primary profile controls and session memory overrides; Layout
only identifies the active profile, while Comparison uses it as target A and owns its session-only
target B picker. The `layout-profile-probe` executable serializes those exact facts into a
deterministic versioned text profile; running a probe built for another target produces an explicit
file the planner can load for the session. Loading never edits LispB or document history.

The version-one format is line-oriented so generated facts stay inspectable and diffable:

```text
ioj-layout-profile 1
name "Windows x64 Debug"
identity platform "Windows"
identity architecture "AMD64"
identity compiler "Clang 21.1.0"
identity build-configuration "Debug"
type "std::uint32_t" 4 4 unsigned 32 "compiler probe"
representation "EntityId" "std::uint32_t"
memory cache-line 64
memory page 4096
memory-provenance "selected machine profile"
```

Identity and memory rows are optional; omission means Unknown. Each type row carries spelling,
nonzero byte size, power-of-two alignment, integer classification, unsigned value bits (or
`unknown`), and its own provenance. Representation aliases may chain to a known fact or an unknown
external spelling, but cycles and duplicate owners are invalid. The loader builds a candidate
profile privately and publishes it only after the whole file validates, so malformed input cannot
partially replace the active target. Session memory overrides remain layered on the selected
profile and **Restore profile facts** returns to that profile's original memory group.
The explicit profile path is UI workspace state stored in the existing ImGui settings section,
keyed by normalized project path and percent-escaped with the same conservative codec as graph
positions. Project adoption first restores the built-in profile, then attempts the saved file;
failure leaves the built-in facts active and publishes a nonfatal diagnostic while preserving the
path for repair. **Use built-in profile** removes only the current project's association. The
expandable primitive-fact table reads `AbiProfile` directly and owns no copied fact catalog.

Record target comparison is a headless operation over two independently produced `RecordAnalysis`
values for the same resolved `TypeId` and element count. It validates the complete unique member
set and each member's semantic type/cardinality before publishing any per-member result, joins
members by name rather than incidental vector position, prefixes each side's diagnostics, and uses
checked optional deltas so unknown or overflowed facts remain Unknown. The GUI owns only the
session-only second `AbiProfile`, path/error presentation, and cached comparison result; neither
profile nor its comparison modifies LispB or semantic history.

Raw-union target comparison applies the same all-or-nothing join to `UnionAnalysis`. It requires the
same resolved union, selected count, and unique alternative name/type/cardinality set before
publishing alternatives. The result retains both analyses and checked element-fact, extent,
conditional-slack, object, aggregate, cache-line, page, straddling, and cache-fit consequences.
Diagnostics are side-prefixed and unknown/overflowed values remain absent; the comparison deliberately
does not infer ABI compatibility from matching sizes.

Tagged-union target comparison extends that boundary with discriminant and tag semantics. Before
publishing alternatives it validates the same type/count, discriminant identity, mapped/unmapped/
sentinel/count-sentinel roles, and unique name/tag/type/cardinality set. Checked target facts cover
discriminant storage, payload union, payload offset, internal/tail padding, complete object,
conditional alternative slack, aggregate footprint, cache/page boundaries, and cache fit. Semantic
tag coverage is displayed as held fixed; the optional explicit distribution remains a separate
session analysis rather than being silently interpreted as a target-dependent performance model.
When present, `compare_tagged_union_distributions` applies the same validated unique tag/alternative/
weight set and selected count to both target layouts. It publishes entries only after the whole join
matches and reports checked exact and expected extent/slack deltas; overflowed exact arithmetic stays
Unknown while numerical expectations remain explicitly distribution-derived.

Packed target comparison follows the same boundary while holding one selected `Variant` fixed. It
validates type, storage override, byte/bit order, invalid code, unique ordered segment identities,
field semantics, widths, named codes, and relationships before publishing per-segment results.
Target-dependent storage facts, MSB-first positions, relationship extents, unused/overflow bits,
aggregate footprint, and cache/page facts may then differ honestly. This is distinct from the
existing variant A/B comparison, which holds one target profile fixed while changing physical
overrides.

Standard-library SoA target comparison also holds the selected `Variant`, capacity, allocation
strategy, and explicit workload fixed. `Analyzer::compare_soa_targets` validates the complete
ordered unique column structure and physical override identity before publishing any column result,
then retains checked column size/alignment/allocation and aggregate allocation/cache/page deltas.
The existing `SoaAccessAnalysis` boundary is reused for the same selected intents, element count,
and multiplicity under each profile; comparison now requires exactly one detail per selected column
before publishing contributions. Unknown or overflowed facts stay Unknown and each side's diagnostics
are prefixed. The GUI caches both comparisons as derived session state and presents them separately
from the variant A/B comparison; no profile, capacity, allocation, or workload input enters LispB.

For existing enums and integer scalars, preview/save reparses the declaration's owned source slice
and performs token-local property replacement when the declaration name and ordered child identities
are unchanged. The same property patcher covers linear quantization, integer varint, fixed-point,
sentinel optional, presence-bit optional, and packed-value declarations. Packed preservation checks
ordered field/reserved kinds and names plus field named-code identities, and patches existing
relationships when their source shape is stable. Relationship add/clear on scalars and stable
packed fields inserts or removes only the owned trailing form, including its leading row comments.
This preserves comments, whitespace, and
unchanged token spelling without creating a second syntax or semantic model. Enum values additionally
derive row-owned source ranges from the reparsed declaration: stable named rows can be reordered or
removed with their leading/trailing comments, while inserted or duplicated rows use the canonical
single-row renderer. When source and semantic row counts match and exactly one same-position name
differs, the enum renderer treats it as a direct rename only if the old name disappeared and the new
name did not previously exist. It reuses the exact owned row and patches the name token; multiple
renames, rename/reorder combinations, and other ambiguous shapes retain canonical fallback.
Standalone integer-scalar codes use the same stable-name block ownership while
keeping scalar properties and the optional relationship outside the ordered region. The shared
scalar/packed code-row patcher recognizes exactly one same-position old-only/new-only name as a
direct rename, reuses its complete owned block, and patches the atomic name token; multiple or
colliding unmatched rows retain conservative canonical-row behavior. Packed fields and
reserved regions use the same bounded-block mechanism keyed by segment kind and name. Stable
segments retain leading/trailing comments and token-local edits to
their types, widths, kinds, named codes, and relationships through reorder or neighboring
insertion/deletion; only new segments use the canonical single-segment renderer. The packed
boundary additionally infers exactly one same-position old-only/new-only segment name when its kind
and complete non-name semantics match, then patches the atomic name inside the original owned block.
Multiple renames, kind changes, or a semantic edit on the renamed segment keep the stable-key/
canonical fallback. Stable packed
fields also rebuild their nested named-code region by code name, preserving retained code blocks
while rendering only new or ambiguous code rows canonically. An empty code region can insert its first canonical
row before an existing relationship block or the parent closing form and later remove it without
touching surrounding source. Unreal projections, ambiguous source shapes, and duplicate or
interleaved relationship forms still fall back to the canonical
declaration renderer. Records and raw unions use bounded named member/alternative blocks so stable
children retain comments and token-local type/count edits across insertion, duplication, deletion,
and reorder; new children render canonically. The same positional one-old/one-new inference patches
one direct child rename inside its original block. Tagged unions use that bounded named alternative
behavior while patching tags and keeping discriminant/export properties outside
the ordered region. Other structural edits retain canonical fallback. Standard-library SoAs use a
bounded named-member region ending before functions, fixed layouts, and single-allocation forms.
Stable members patch relationship add/edit/clear through the same nested-form preservation helper;
the relationship is shared semantic metadata and does not change generated storage. Members can be
directly renamed in place only when kind, type, properties, mask dimensions, and relationship all
match the source row; the original owned block then receives one atomic name-token patch.
Mask-dimension properties have their own nested boundary: uniquely named multiline dimension forms
retain row comments through extent edits, insertion, deletion, and reorder, while one same-position
rename reuses its block only when the extent is unchanged. Compact lists patch stable name/extent
tokens directly and canonicalize only the property value for structural ambiguity. Members can be
structurally edited only after those advanced forms are proven semantically
unchanged; unsupported derived allocator/mutable-view surfaces take canonical fallback. Fixed-
layout changes are localized separately: an existing fixed form is replaced or removed at its
owned form range, and a newly enabled fixed form is inserted after custom functions and before
single-allocation output. Existing fixed forms now patch the storage atom and `:containers` region
locally. Multiline lists use uniquely named source blocks so stable container comments survive
insertion, deletion, reorder, and one old-only/new-only direct rename; compact lists retain local
name-only patches and otherwise canonicalize only the list value. This preserves unrelated
declaration comments, member formatting, and opaque custom function text.
An existing single-allocation form has a narrower boundary: its owner atom is patched in place and
its variant region uses uniquely named owned row blocks. Stable variants retain comments and
spacing through allocator edits, insertion, deletion, and reorder; one same-position old-only/new-
only variant name reuses its block only when the allocator is unchanged. Ambiguous rows fall back to
canonical row rendering, while enabling or disabling the complete form remains a localized
canonical insertion/removal.
The source boundary has a shared scalar-list path for atom and quoted-string entries. Top-level
`:operations` atoms retain unique multiline operation blocks and comments through capability
insertion, deletion, reorder, and one direct substitution; first add/last remove use the ordinary
property path. The `(all)` shorthand remains exact while the resolved operation set is unchanged
and expands to explicit atoms only when a capability changes. Enum `:conversions` atoms use the
same boundary and shared canonical conversion ordering/names, so inline policy changes preserve
stable unique multiline blocks while compact or ambiguous lists canonicalize only the bounded
property value. Top-level `:using-declarations`
similarly treats unique quoted values as source-owned rows. Stable opaque fragments retain comments
and escaping through insertion, deletion, reorder, and one direct edit; compact lists patch one
value locally and otherwise canonicalize only the list. Duplicate or ambiguous values deliberately
take that localized canonical fallback and never become graph relationships.
Custom-function list-form `:body` fragments and registered `:dependencies` keys reuse that quoted-
value boundary within the function's owned block. Stable unique multiline entries therefore retain
their comments, spacing, and escaping through insertion, deletion, reorder, and one direct edit.
Missing or emptied properties still use the ordinary property insertion/removal path, duplicate or
ambiguous lists canonicalize only their bounded value, and unchanged raw `#cpp` bodies remain exact.
Editing a raw body deliberately converts only its property value to the quoted-list representation;
opaque body fragments are not parsed as C++ and dependency keys are not semantic graph edges.

The enum editor likewise operates directly on `EnumSchema`. Reflection mode, enum-array emission,
native API mode, export specifier, and conversion functions all submit ordinary `ReplaceEnum`
commands. Shared validation is the sole authority for incompatible native/Unreal policy, global
reflection, count-sentinel, and output requirements. Reflection and conversion source spellings
and ordering come from shared schema helpers; the UI does not retain a competing policy model.
The optional `EnumUnrealProjection` is edited as one complete shared nested schema rather than a
planner projection. Absent-form fields are merely staging buffers until all required values exist;
add/edit/remove operations submit `ReplaceEnum` and shared validation enforces the native plain-enum
contract. The source boundary patches an existing projection name/properties in place or performs a
localized whole-form insertion/removal, preserving surrounding enum values and comments.
Packed-value, record, raw-union, and tagged-union editors follow the same rule for their optional
export specifiers: a declaration-local input commits through the corresponding typed `Replace*`
command, and shared identifier validation owns rejection. Their established source boundaries
patch the property without taking ownership of child rows or comments.
The packed editor's mutable-API checkbox likewise changes only
`PackedValueSchema::mutable_value` through `ReplacePackedValue`. It is a C++ API-generation policy,
not semantic-domain or target-layout state; the shared lowering remains the sole owner of the
resulting `try_make`, `try_set_*`, and setter surface.

Declaration rename is a typed document transaction rather than text replacement. Supported enum,
integer-scalar, representation, packed-value, record, union, and ordinary SoA declarations retain
their `DeclarationId`, discover users from the resolved graph, repair the corresponding shared-
schema `TypeRef` fields to an unambiguous qualified spelling, resolve the whole candidate graph, and
render every affected top-level declaration through the source-aware boundary. When the only reason
the normal preservation parser cannot recognize the owner is the validated new declaration name,
the boundary patches that one atomic name token in an exact copy and retries the same kind-specific
renderer. Comments, custom spacing, stable child blocks, and repaired user references therefore
remain localized; malformed or unsupported shapes still take canonical fallback. SoA rename also repairs module-local
`nested-schema`/`fixed-schema` identities. Explicit view, mask, fixed-storage, and allocation helper
names remain stable; implicit generated names follow the renamed declaration, and collisions roll
the transaction back. Registered aliases remain rejected because the types registry does not yet
participate in source-aware edits; vector-SoA modules are not ordinary editable declarations.

SoA declaration duplication is also prepared by the shared editable document rather than by ImGui.
Preparation is non-mutating: it copies the source schema, selects a declaration name whose implicit
view and single-allocation helpers do not collide, derives unique explicit view, field-mask/enum,
fixed-layout, single-allocation, and allocator-variant names, and repairs the copied mask-storage
member's generated type reference. The UI then submits the ordinary typed `CreateSoa` command, so
validation, history, preview, save, and reload remain on the same semantic path as simple SoAs.
External member types, nested schemas, and allocators remain references to their original semantic
dependencies; arbitrary custom function bodies remain opaque source text rather than a second
planner-owned schema.

The SoA inspector treats view names as optional source policy rather than mandatory C++ facts.
Clearing an explicit mutable or const view name restores the schema-derived name; enabling the
explicit form initially records that same effective identity, after which ordinary inline editing
can select a distinct generated name. Both transitions use `ReplaceSoa` and the declaration-local
property patcher, so they participate in shared validation/history without rerendering unrelated
members or custom functions.

Single-allocation owner editing follows the same document-owned pattern. Name selection reserves
the owner and its generated `Storage` identity as a pair; shared manifest validation remains
responsible for the schema-derived layout/view names. Adding, renaming, or removing the durable
single-allocation form patches only that owned form range, preserving surrounding comments and
custom functions. Owner changes retain allocator variants. The inspector will not disable a form
while variants remain, avoiding silent destructive loss before the inline variant editor handles
their explicit removal. The variant editor stages a valid allocator reference before insertion, so
it never needs an unresolved placeholder declaration. Variant owner names reserve their generated
`Storage` partner, and all direct edits, duplication, deletion, and reorder operations replace the
shared SoA schema through normal document history rather than maintaining UI-owned semantic rows.

Deletion is likewise enforced by the shared editable document rather than only by the frontend.
Every typed delete rejects a declaration with resolved reverse users before mutating the manifest,
or one bound to a registered alias, then validates and re-resolves the remaining candidate graph.
For source-backed declarations the document retains the exact top-level source range as history
metadata: preview removes that range, undo restores its ownership and stable declaration identity,
redo removes it again, and successful save/reload clears the tombstone. The UI uses the same
destructive confirmation for source-backed and newly created declarations.

`MoveDeclaration` relocates an editable declaration between compatible existing modules without a
delete/create identity break. The command requires the same module kind and moves the shared schema
transactionally. When the destination namespace differs, it rewrites supported graph-derived
`TypeRef` users to the new qualified spelling before re-resolution; registered aliases and
module-local SoA nested/fixed references that cannot cross the module boundary reject the complete
transaction. Collisions and invalid target configurations likewise roll back without dirty/history
movement. Source-backed moves retain their original top-level slice as owned history metadata:
preview deletes that slice and inserts its preserved text at the requested destination position,
including across source files; affected user declarations preserve their normal local source
editing behavior, and undo restores the original ownership and reference spellings. Enum and scalar
domain definitions resolve before dependent representations/tagged unions, so legal moves do not
acquire a source-file-order dependency. Newly created unsaved modules must be saved before receiving
an existing source-backed declaration because their whole-module renderer has no independent child
source ownership yet.

Enum, standalone integer-scalar, linear-quantized, integer-varint, fixed-point,
optional-sentinel, optional-presence-bit representation,
packed-value, record, raw-union, and
standard-library SoA authoring are write-enabled vertical slices. They can add declarations to
existing matching modules, edit ordered
child rows through semantic commands, preview affected sources, and explicitly save and reload
LispB. Packed values store an ordered shared-schema variant of semantic fields and named reserved
regions; reserved regions have no type dependency and emit no value API. Packed fields and SoA
columns preserve semantic type links while the layout view derives physical facts. Drag reorder and
packed dividers issue ordinary undoable schema commands. Planner capacity remains session state
rather than becoming a SoA source property, and there is no persistent variant format.

`CreateModule` extends document history to source-owned module containers. It appends a validated
empty editable module to an existing loaded module source, records pending source ownership
separately from semantic declarations, and canonically renders the whole new module so declarations
added before first save appear inside it. Undo removes only the newest still-empty pending module;
normal declaration undo therefore restores that precondition. Save validates the temporary source
set and reloads ordinary parsed module/source ranges, eliminating pending state. Empty editable
modules are legal generation destinations. `EditableProjectDocument` owns exact project-manifest
source, the parsed `Project`, source-list ranges for `cpp-schema` targets, pending new-source
contents, and typed add/remove/create/discard inversion. It validates the complete resolved target
before accepting an existing file, new file, or unregistration and preserves all text outside the
owned `:sources` list. Removing an original source erases only its string token from the manifest;
the file and surrounding source-list comments remain untouched.
New files remain absent from their destination until Save; preview returns both the exact manifest
edit and each complete new file. Save writes all candidates to temporary paths, validates them as
one project, publishes new sources before atomically replacing the manifest, rolls published
sources back if manifest replacement fails, and reloads clean ownership/history. An empty published
source is deliberately valid when the target already contains a module, allowing the ordinary
`EditableSchemaDocument::CreateModule` command to author its first module after reload. The planner
loads this project document beside the schema document. Its Project view lists and unregisters
current sources, registers existing relative sources, or stages empty ones through shared commands,
and the persistent Source view lists their loaded files and composes both documents'
affected-file previews. Project-source history is intentionally exclusive with semantic history: while it
is active, schema commands and Save As are disabled, global Undo/Redo address the project draft,
and Save publishes then reloads both documents. Project switching and close protection check both
dirty states. A fully undone project history can be redone or explicitly discarded by reload.
Widgets never pre-create files or mutate the manifest themselves.

Nested SoA columns edit optional fixed-schema and nested-schema references through the same
whole-schema command path, and array selection clears those nested-only properties before
validation. Any SoA column can also carry the shared optional semantic relationship used by packed
fields, integer scalars, and record members. It participates in dependency labels, rename/deletion
safety, source-preserving commands, and navigation while capacity/allocation remain session state.
The coordinated mask workflow enables or disables the generated mask type, field enum,
storage column, and field membership atomically so intermediate invalid configurations never enter
history. Per-field mask dimensions use persistent UI edit buffers but commit only through normal
whole-schema commands, keeping validation and undo/redo in the editable document.

The application treats a C++ schema target as the open authoring document. Open/recent operations
load its project manifest and target, while Save As materializes the current draft as a validated,
self-contained one-target project clone. The browser follows the semantic manifest's module order
and declaration ownership rather than inventing type-kind folders.

## Supported analysis and limits

Ordinary AoS semantics use `record-module` / `record` declarations rather than overloading SoA
columns. A record member references the shared semantic type graph and may have a fixed element
count or an optional shared semantic relationship to another declared type; C++ lowering is one
consumer and emits an ordinary struct plus `std::array` where needed. Relationships participate in
dependency/rename/deletion safety and graph labels but do not alter C++ member storage.
Physical member offsets and padding remain target analysis, not durable semantic properties.
The record analyzer recursively derives nested-record facts, fixed-array extents, offsets, object
alignment, and internal/tail padding from the selected ABI profile. Unknown target facts stay
Unknown, while illegal by-value record cycles are rejected during semantic resolution.
Record create/replace/delete commands share the same stable declaration identity, validation
rollback, undo/redo, preview, and atomic save/reload path as enum, packed, and SoA declarations.
The inline type chooser derives valid local names, uniquely qualified cross-module declaration
spellings, and registered `@name` references from the shared document/type graph, plus target
primitive spellings from the ABI profile. Ambiguous cross-module spellings remain visible but are
not selectable, so they cannot silently become external leaves. It does not own a parallel type
registry.

Packed-field contextual enum creation stores only a declaration ID and field name as transient UI
workflow state. The existing `CreateEnum` command publishes the shared declaration, then one
`ReplacePackedValue` command binds the field and clears incompatible integer-only metadata. These
are deliberately two ordinary history entries, so Undo first restores the old field and then
removes the enum. A rejected bind immediately undoes the just-created enum before presenting the
diagnostic; no planner-owned enum or compound schema model exists.

SoA-column contextual record creation follows the same boundary. Its transient context is only the
owning SoA declaration ID and stable column name. `CreateRecord` creates a shared record with the
column's prior semantic type as its initial member, then `ReplaceSoa` binds the column using an
unambiguous qualified spelling while retaining kind and all other SoA metadata. The commands remain
independently undoable, and a failed bind immediately undoes the record creation.
Record aggregate analysis uses the planner's selected element count and target profile to report
total storage, member extents, ABI padding, minimum cache lines, and minimum pages. Arithmetic
overflow and absent target facts remain explicit diagnostics/Unknown values.
Element straddling counts are exact for the stated model of a contiguous, region-aligned array;
they use stride/region periodicity and are physical facts rather than performance predictions.
The initial workload slice exposes a session-only set of record members, defaulting to the focused
member, as an explicit sequential read set. It computes the union of all selected member byte
intervals and target cache-line/page intervals with a bounded periodic algorithm, without double
counting overlapping regions. It reports useful versus non-selected bytes and does not turn those
physical facts into a performance score.
The same record can also be analyzed under two target profiles and compared through a typed
`RecordTargetComparison`. Object/member/aggregate deltas include alignment and padding as well as
cache-line/page boundary facts. Semantic member mismatch rejects the comparison transactionally;
missing target facts propagate as Unknown rather than falling back to either side.
`RecordAccessComparison` then applies one complete session-only member-to-operation map, element
count, and multiplicity to both target layouts. Record identity and the entire unique workload are
validated before useful/read/write/logical bytes, enclosing AoS footprint, exact cache/page address
coverage, non-selected footprint, and cache-fit consequences are exposed. The analysis preserves
the intended workload identity even when a target cannot produce offsets, so missing physical facts
remain Unknown rather than becoming a false workload mismatch.

Raw unions are separate shared `union-module` declarations with ordered semantic alternatives and
optional fixed counts. They lower to ordinary C++ unions, while one aggregate-cycle validator
rejects direct and mixed record/union by-value recursion. Target analysis chooses the maximum
alternative extent and alignment, rounds the object size, and reports tail padding plus each
alternative's union slack. Selected-count analysis assumes a contiguous, cache-line/page-aligned
array and scales total storage, tail padding, and each alternative's conditional slack with checked
arithmetic. Conditional slack does not imply a runtime tag distribution.

An optional frontend workload maps stable raw-union declaration IDs to explicit alternative
weights. `analyze_union_distribution` validates unique known alternative names, retains per-entry
extent and conditional slack, and derives checked weighted totals plus numerical per-value and
selected-count expectations only from positive weights with complete target facts. Exact overflow
and missing facts remain Unknown without discarding valid row facts. `compare_union_distributions`
transactionally requires the same union, selected count, complete unique alternative set, and exact
weights before publishing any row or aggregate delta; each side's diagnostics retain target
provenance. This workload is cleared with the project document, never enters LispB, and does not
invent a raw-union discriminant. Before replacing the resolved workspace graph,
`sync_document_graph` reconciles old semantic alternative names with the new shared document: one
unambiguous rename migrates its weight, deletions prune stale keys, and reorder/property edits retain
identity. The same reconciliation runs after apply, undo, and redo.

Tagged unions are a distinct shared declaration and semantic node rather than a planner annotation
or mode bit on a raw union. A tagged declaration references an enum discriminant and maps each
ordered payload alternative to one declared non-sentinel enumerator. Validation rejects missing,
duplicate, unknown, and sentinel tags plus non-enum discriminants. The graph exposes a
`discriminates` dependency and tag-labeled payload edges. Initial C++ lowering is an explicit
discriminant member followed by a nested raw payload union. Target analysis follows exactly that
lowering: it derives tag facts from the enum's backing representation, aligns the payload union,
then reports the inter-member gap, shared payload size, per-alternative conditional slack, tail
padding, total size, and alignment. Nested records/raw unions/tagged unions reuse those derived
facts, and mixed by-value cycles are rejected before lowering or analysis.
Typed create/replace/delete commands retain stable declaration identity and use the same graph-wide
validation rollback, history, source preview, atomic save, and reload path as other declarations.
The frontend's creation flow and flat alternative editor are command clients only: discriminator,
symbolic tags, types, counts, and ordering remain exclusively in the shared tagged-union schema.
Changing an enum declaration name repairs tagged discriminant references atomically.
Selected-count analysis uses the same documented contiguous, cache-line/page-aligned array model as
records and raw unions. It separately scales object storage, discriminant bytes, payload-union
storage, inter-member padding, tail padding, and each alternative's conditional payload slack.
Region crossings and cache fit derive from the target profile. A conditional slack total means all
objects carry that tag; it is not an inferred runtime tag distribution.
The analyzer also classifies declared discriminator symbols without changing layout: payload-mapped
live tags, unmapped live tags, named sentinels, and the count sentinel. Payload mappings continue to
reject either sentinel role. Unmapped live tags mean the tagged declaration has no payload case for
that declared state; they are not assumed to be errors or byte waste. Numeric codes with no enum
symbol remain the enum-domain analyzer's separate unused-code-space fact.
An optional frontend workload maps stable tagged-union declaration IDs to explicit per-tag weights.
Headless analysis validates those tags, retains checked weighted row/sample extent and slack totals,
and computes numerical expected values only when positive valid weights and complete target facts
exist. Overflow makes exact totals Unknown without discarding inspectable row facts. This workload
cannot alter fixed ABI size or semantic schema and is cleared when a project document is replaced.
Graph synchronization likewise reconciles the old and new symbolic tag sets: a single tag
reassignment migrates its weight, deleted tags are pruned, and alternative name/order/type/count
changes leave the tag-keyed workload untouched.

The planner supports enum value-domain analysis, packed storage type/bit-range analysis, and flat
standard-library SoA/vector-SoA payload analysis. A shared schema-layer domain engine derives enum
literal/implicit values, named- and count-sentinel code use, signedness, and minimum width. LispB's
optional `:bit-width` is a durable semantic constraint (`auto` when absent). The positional C++
underlying type is now optional and, when present, remains only an explicit lowering preference.
When absent, shared domain analysis derives signedness and effective semantic width, while C++
lowering selects the smallest legal fixed-width 8/16/32/64-bit primitive. Unknown signedness or
width rejects derived lowering rather than inventing storage. The resolved semantic enum has no
dependency edge to a fabricated primitive; target analysis reports the separately derived backing
facts. Per-value `:sentinel true` metadata is
durable and may represent multiple reserved semantic states without conflating them with allocated
storage waste.
`EnumTargetComparison` applies two explicit target profiles to that one resolved enum. It first
checks type identity, backing spelling, all semantic domain facts, and the complete ordered set of
names, resolved codes, and sentinel roles. Only then does it publish backing size, alignment,
storage-bit, unsigned-value-bit, fit, and unused-backing-code deltas. Side diagnostics retain their
target identity, missing facts remain Unknown, and semantic width/code slack remain unchanged
domain facts rather than being reinterpreted as ABI storage. Unsigned backing code capacity follows
the profile's value-bit fact rather than assuming every storage bit is a value bit.
`EnumBackingAggregateAnalysis` applies the shared selected count only to the generated standalone
C++ backing. Checked total bytes, target cache/page footprints, aligned-contiguous straddling, and
cache fit can therefore be compared across targets without treating semantic width as `sizeof` or
assuming that an enum used inside a packed value occupies standalone backing storage.
Optional `:signed` is another semantic-domain constraint; when absent, signedness is inferred from
known values. Width derivation consumes the semantic signedness rather than the backing primitive.
Known width mismatches are rejected during semantic
edits; arbitrary initializer expressions remain explicitly unknown. Packed analysis reports overflow-safe aggregate
storage, semantic payload bits, explicit reserved bits, implicit unused bits, cache lines, pages,
and aligned-contiguous cache-line/page boundary-straddling counts at a selected element count. The
built-in profile records CMake-supplied platform, architecture,
compiler, and build configuration identity independently from its physical-fact provenance. ABI
identity remains Unknown until a factual identifier is supplied. Compiler-derived size/alignment
facts identify their source, while the x86/x86-64 baseline memory facts separately identify the
source of the explicit 64-byte cache-line and 4 KiB page assumptions. SoA cache tiling consumes the
same profile fact rather than an analyzer constant. Missing
profile facts, unknown physical types, and nested SoAs produce diagnostics and Unknown results
rather than guesses.

The planner may override `MemoryFacts` for the current session. Cache-line/page/cache-capacity
inputs remain part of the explicit target profile consumed by analysis; they are not schema
properties, do not enter `EditableSchemaDocument` history, and do not dirty or serialize LispB.
Empty inputs restore Unknown rather than guessing a local machine value, while restoring profile
facts recovers the original provenance-bearing group.

Selected standard-library SoA access analysis retains per-column physical widths and derives
cache-line/page boundary-crossing counts at the workload element count. Those counts are
conditional physical facts for a region-aligned origin per separate column allocation. Missing
region or type facts remain Unknown, and physical-variant comparison preserves the per-column
counts and checked deltas without treating them as measured traffic or performance.
Each selected packed field, record member, or SoA column carries a shared read, write, or read+write
access intent;
the session-wide operation control supplies and bulk-updates that intent. Analysis keeps unique
useful payload independent from classified read/write useful bytes, so mixed sets and read+write
fields may contribute to either or both categories without being double-counted as storage.
Cache/page footprints remain address-coverage facts and deliberately do not model write allocation,
eviction, or bus traffic. A positive multiplicity scales only overflow-checked logical read/write
useful-byte totals; it does not multiply unique payload, cache lines, pages, or allocation capacity.
The analyzer also derives read and write address coverage independently: exact region unions for
the aligned contiguous AoS model and minimum per-column region sums for separate SoA allocations.
Read+write fields participate in both, and multiplicity never scales either address set.
Comparisons require the same complete name-to-operation mapping and multiplicity.
Packed access analysis uses bits for useful payload because fields need not be byte-aligned, but
uses the complete contiguous backing-word array for physical storage and region coverage. A selected
field therefore cannot reduce the minimum cache-line/page address set below its containing packed
elements. Reserved regions are not selectable; unknown profile facts and checked overflow remain
Unknown with diagnostics. This workload state is GUI/session state and never enters LispB.
`compare_packed_access` accepts only analyses with the same complete field-intent mapping, element
count, multiplicity, and packed type. It retains each side's checked analysis and derives deltas only when both
facts are known, so a field-width override changes useful-bit facts while a storage override changes
the containing-word array and its cache/page coverage. The frontend presents those physical facts
without inferring independent bitfield fetches, traffic, or performance. Each analysis also retains
checked per-selected-field bit contributions. Comparison joins those details by stable field name,
independent of input order, and rejects missing detail rather than silently presenting an incomplete
aggregate explanation. The target-profile path first validates the active packed variant through
`PackedTargetComparison`, then applies that same workload under profiles A/B and reuses the checked
access join; it remains distinct from changing variants under target A.
For a valid physical packed layout, non-useful storage is decomposed into ordinary unselected field
bits, explicit reserved-region bits, and backing bits outside every declared segment. All three are
scaled with checked arithmetic and verified against `storage footprint - selected useful bits`.
They remain separate because reservation is a semantic code/region policy, whereas unused backing
bits are physical slack; an invalid/overflowed aggregate makes the decomposition Unknown.

SoA allocation strategy is explicit session analysis state. `separate_columns` preserves the
standard-library model of one independently aligned allocation per column and reports only summed
lower-bound cache/page footprints. `aligned_contiguous` lays out every capacity-sized column in
declaration order inside one region-aligned block, using target `TypeFacts` alignment to derive
column offsets, inter-column padding, block alignment, and total allocation bytes. Selected column
prefixes can therefore be unioned into exact cache/page address coverage for that stated block
origin. The choice and capacity remain outside LispB and editable-document history. Allocator
headers, growth policy, heap/OS overhead, and performance effects remain Unknown rather than being
inferred from either strategy.
The contiguous Layout view consumes those same offsets and region facts in a horizontally
scrollable cache-line/page map. It draws full capacity extents, alignment gaps, target-region
boundaries, and read/write/read+write selected prefixes. Zoom and bounded-canvas limits are UI state
only; no layout arithmetic or semantic schema is owned by the widget.

Packed physical ordering belongs to `PackedValueSchema` / resolved `PackedType`, not to semantic
field types or the target profile. Absent `:bit-order` means the historical LSB-first segment
allocation; explicit `msb-first` allocates declaration-order segments downward from the storage
MSB and drives both generated numeric masks/offsets and analysis. Optional `:byte-order` records the
little/big order of an external serialized byte sequence. It does not change C++ object ABI or
claim a serializer exists. The editable document token-locally preserves both properties, while
the planner keeps serialized byte ordering visually distinct from numeric storage bit positions.

`MemoryFacts` may also provide L1 data, L2, and L3 capacities. Aggregate packed storage, record
storage, and standard-library SoA column payload compare their selected-count working set directly
with each supplied capacity. Fit remains Unknown when either the aggregate size or capacity is
unknown. These are capacity comparisons, not performance predictions, and the built-in baseline
does not guess capacities for the machine running the planner.

`scalar-module` / `integer-scalar` is the first schema declaration that describes an integer value
domain without selecting a C++ underlying type. Its shared schema and resolved `IntegerScalarType`
store signedness, inclusive live bounds, optional explicit width (`auto` when absent), and ordered
named live/sentinel codes. Validation derives fit across the range and code extremes; headless
analysis reports live, sentinel, required, and unused code space, including exact zero waste for
full 64-bit domains whose mathematical value count cannot fit `uint64`. The resolved node has no
target size or alignment. Its configured generated header is intentionally empty except for an
optional prelude by default: no physical C++ type is fabricated. A schema-owned optional C++
projection policy can emit named codes as typed `inline constexpr` constants. Validation requires a
supported integral output type, verifies the complete live/sentinel domain fits, and rejects
generated-name collisions. This policy is consumed only by lowering; it does not enter
`IntegerScalarType` or create target layout facts. Its opt-in `constants-with-names` form adds a
`constexpr` switch returning `std::string_view`; unnamed values return an empty view, so generated
metadata never implies that every value in a range has a symbolic name.

Packed integer fields may resolve their semantic type directly to an `IntegerScalarType`. In that
case validation requires the packed kind to match scalar signedness, derives auto width from the
scalar's effective range/code extremes, and rejects field-local ranges or codes that would create a
second competing domain. The resolved `PackedField` retains the scalar `TypeId` dependency and
publishes scalar-derived range/code facts for layout analysis. Packed C++ lowering alone selects the
smallest `std::intN_t`/`std::uintN_t` capable of holding the concrete placement width and enforces the
shared live/sentinel domain. This consumer policy does not add size, alignment, or a primitive
dependency to the standalone scalar node.

`representation-module` / `linear-quantized` is the first explicit semantic-to-physical link. A
`LinearQuantizedType` resolves its `integer-scalar` source as a real graph dependency while owning
only representation facts: encoded width, reserved-code count, and reject/clamp clipping policy.
The initial mapping assigns the source endpoints to the first and last usable codes; reserved codes
are excluded from the upper end of code space. Analysis uses exact integer capacity checks and
long-double numerical consequences to report resolution and half-step maximum rounding error.
The source may be signed or unsigned. Exact sign-and-magnitude span calculation happens before the
numerical conversion, so negative-only, cross-zero, and full signed 64-bit domains do not pass
through an unsigned cast or overflowing signed subtraction.
Because `2^64` cannot be stored in `uint64`, the analysis has an explicit exact `2^64` code-count
case rather than reporting a known capacity as Unknown; resolution is calculated from the same
mathematical power of two.
The configured generated header remains empty because the declaration does not choose a standalone
ABI wrapper. A packed field may, however, place the representation explicitly with
`:kind linear-quantized`. That placement keeps the representation `TypeId`, derives exactly its
encoded width, and rejects competing field-local range, code, helper, or relationship facts.
Packed lowering selects only a fixed-width unsigned carrier for the concrete placement and exposes
it through deliberately named `*_encoded` construction/getter/setter APIs. High-end reserved codes
are rejected by construction, mutation, and `is_valid`; no generated API pretends that an encoded
code is the decoded source value. Headless packed analysis embeds the existing quantization analysis
so the UI consumes the same source range, capacity, resolution, error, endpoint, and clipping facts.
Quantize/dequantize helpers and non-packed container placement remain later policies.
`Analyzer::compare_linear_quantized` compares two declarations only when their resolved source
`TypeId` matches. It carries both analyses and derives exact width/code-space deltas, numerical
resolution/error deltas, clipping-policy changes, and overflow-safe encoded payload bits for the
selected element count. It deliberately does not turn those payload bits into allocated bytes or
cache/page facts: doing so requires a future container placement and packing policy. The frontend
stores the selected sibling by stable `TypeIdentity`, not a planner-owned copy of either schema.

`representation-module` also supports source-backed `integer-varint` declarations with unsigned,
signed (SLEB128-style), and ZigZag encodings. Validation binds each declaration to an
`IntegerScalarType` and enforces encoding/source signedness. Analysis derives the exact minimum and
maximum encoded byte counts over the live range and all named source codes, then scales both bounds
with checked arithmetic. It never exposes a fixed ABI size. Optional explicit value/weight
distributions are session workload state keyed by stable semantic identity; headless analysis
validates each value against the live range or named codes and computes checked exact sample totals
plus a distribution-based expectation. Each valid input also retains analyzer-owned bytes/value and
checked weighted-byte facts so the frontend never reimplements an encoding algorithm. Those weights are not schema semantics and are not written
to LispB. Representation
modules may mix quantized and varint forms; editable-document source ownership matches forms by
kind and declaration name while the resolved graph retains one stable declaration identity per
form.
`fixed-point` is a standalone physical numerical representation with explicit signedness, total and
fractional widths, and nearest-even or toward-zero rounding. Its semantic node has no invented C++
primitive or ABI facts. Headless analysis derives the exact integer raw range, power-of-two scale
and resolution, numerical endpoints, rounding-error bound, and checked selected-count encoded bits;
allocated bytes, alignment, cache, and page consequences remain Unknown until placement/lowering is
declared. A packed field may explicitly place the representation with `:kind fixed-point`. The field
retains the representation `TypeId`, derives exactly its total width, rejects competing integer
domain/helper/relationship facts, and ignores session width overrides with an explanation. Packed
lowering chooses only the smallest signed or unsigned native carrier for the scaled raw integer and
emits deliberately named `*_raw` APIs plus exact raw boundaries. It does not fabricate a standalone
representation wrapper or claim to decode a real value. Packed analysis embeds the same
`FixedPointAnalysis`, so the UI does not reimplement scale, range, resolution, or rounding facts.
Conversion helpers and non-packed placement remain later policies.

`mini-float` is an explicit binary floating-like representation with 0-or-1 sign bits, bounded
exponent and significand widths, and a signed exponent bias. The initial policy reserves the
all-zero exponent for zero/subnormals and the all-one exponent for infinity/NaN. Its shared semantic
node contains encoding facts only: no C++ primitive, ABI size/alignment, byte order, or arithmetic
conformance is inferred. Headless analysis retains exact exponent/code-role facts and treats host
`long double` overflow or underflow as Unknown numerical endpoints with diagnostics.
Typed create/replace/delete commands, exact source tombstones, duplication, rename, flat inline
editing, preview, save, and reload use the shared editable document. Stable property-only edits use
the same token-local renderer as other simple representations, preserving declaration comments,
whitespace, and unchanged spelling; new declarations and unsupported structural changes use the
canonical renderer.
Packed `:kind mini-float` placement retains the shared representation identity, derives exact
width, rejects competing integer-field facts, and ignores session width overrides with a warning.
The generated API exposes only encoded unsigned code bits, including the infinity and NaN
patterns. Packed analysis embeds the shared `MiniFloatAnalysis`; the UI must show Unknown if a
numerical endpoint is unavailable rather than manufacturing a finite value. No decoded floating
arithmetic or standalone compiler ABI is inferred from this placement.
`optional-sentinel` is a distinct source-backed encoding policy over an `integer-scalar`. It names
one existing source code whose shared metadata marks it as a sentinel, resolves the exact code value
and source-derived bit width, and adds a semantic dependency edge. The source scalar remains the
sole owner of its live domain and named codes; selecting an absence sentinel neither copies nor
mutates that domain. Headless analysis separates present values, the single absence code, other
named sentinel codes, and remaining unused codes, then scales encoded payload bits with checked
arithmetic. It does not fabricate a standalone C++ type, ABI size, alignment, or allocation.
Typed create/replace/delete commands, scalar-rename repair, exact source tombstones, duplication,
flat constrained editing, preview, save, and reload use the same shared document path as other
representations.
`optional-presence-bit` is a sibling physical policy over the same `integer-scalar` source. Its
semantic node retains the source payload width and derives one additional presence bit, including
an honest 65-bit encoding for a 64-bit source. Analysis exposes one canonical semantic absence
state, source sentinels and unused payload codes, redundant noncanonical absent payload patterns,
and checked aggregate bits. Those redundant patterns are encoding multiplicity, not extra semantic
states or allocated padding. Bit order, byte packing, ABI size/alignment, and cache/page facts stay
Unknown until a placement policy supplies them. It has its own source syntax and typed commands;
neither optional representation is a mode flag that changes the other declaration's meaning.
`Analyzer::compare_optional_encodings` projects either policy into a policy-neutral factual summary
only for analysis. Same-source comparison retains which sentinel is consumed, remaining source
sentinels, unused payload codes, redundant absent patterns, encoded width, and checked scaled bits;
the durable declarations remain distinct graph nodes and no ABI storage is inferred.
Future tagged optional encodings should likewise remain separate comparable representations.
`Analyzer::compare_integer_varint` requires matching resolved sources and compares the two exact
per-value and scaled lower/upper byte bounds. Signed and ZigZag encodings may have identical bounds
while differing in code mapping; the comparison reports that fact without inventing a compression
ranking. `compare_integer_varint_distribution` applies one source-keyed explicit workload to both
encodings and reports checked sample-byte plus numerical expectation deltas. The frontend never
compares independent per-representation workloads as though they were equivalent.

Packed signed and unsigned field ranges are shared source semantics (`:minimum` / `:maximum`), not planner
annotations. Validation and generated setters enforce them. Layout analysis derives code-space facts
but reports unused representable codes separately from physical unused or reserved bits. Ordered
field-local `(code Name :value N)` children provide named ordinary-integer constants; optional
`:sentinel true` codes must be outside the live range and remain accepted by setters and raw-value
validation. Analysis counts live and sentinel codes separately and derives direct-encoding width
from the actual highest required code. This is intentionally field metadata rather than a fake enum;
standalone named-value semantics use `integer-scalar` instead.
Packed signed fields are likewise shared semantics (`:kind signed`) and may use any explicit or
range-derived width
from 1 through the referenced signed integer type's native width. The resolved graph retains the
physical width, analysis derives the exact two's-complement range, and lowering emits range-checked
setters plus magnitude-based sign extension that handles the minimum value without signed overflow.
This is an arbitrary-width packed representation, not a fabricated standalone C++ primitive type.
Packed field source widths are optional: an explicit integer is durable physical intent, while
`:bits auto` asks shared schema resolution to derive a concrete width. Integer fields derive from
the signed or unsigned extremes across their local range and named codes, or from a referenced
integer scalar's shared domain; enum fields consume the shared enum domain width; linear-quantized
fields consume their representation's exact encoded width. The resolved
`PackedField` retains an auto-provenance flag alongside its concrete
width, so validation, layout, variants, visualization, and C++ lowering never need a magic numeric
sentinel for "auto".
The contextual packed-field scalar workflow is orchestration over those same shared commands, not a
new model. It creates an `IntegerScalarSchema` from the field's current range, signedness, width
policy, and named codes, then applies a separate `ReplacePackedValue` that clears only the duplicated
field-local domain. Physical placement width and field relationship remain on the field. A failed
bind immediately undoes scalar creation, while a successful pair stays as two ordinary history
steps and follows the existing preview/save/reload path.
Packed semantic fields and standalone `integer-scalar` declarations may own one source-backed
relationship with a declared target. `SemanticRelationSchema` and `SemanticRelationship` are shared
schema/graph values rather than packed- or planner-specific annotations. Kinds include `index_into`,
`count_of`, `offset_into`, `discriminates`, `contains`, `member_of`, `quantises`, `encoded_as`, and
`references`; targets resolve through the same `TypeRef`/`TypeGraph` machinery as other semantic
references. Targets that resolve only to external C++ leaves are rejected: a semantic edge must
terminate at a declaration. The relationship adds dependency/user edges and an understandable graph
label. Rename repair, deletion safety, editable-document history, source preview, and save/reload use
those shared identities. C++ lowering remains unchanged because the relationship is semantic
metadata, and an integer scalar still has no mandatory standalone ABI type. The analyzer derives
`RelationshipTargetFacts` from the resolved graph, physical variant, ABI profile, default
capacity, and selected SoA allocation strategy; the UI supplies those session inputs rather than
walking the graph or reconstructing physical facts. Element capacity and byte allocation extent
remain independently optional. For an unsigned
`index_into` SoA it models live indices
`0..capacity-1`; for `count_of` it models live counts `0..capacity`. Named sentinels add required
code states, exact `2^64` is retained without overflow, and analysis reports the resulting minimum
width and whether the current packed/scalar width suffices. Each physical variant supplies its own
effective SoA capacity. Packed thresholds appear in Comparison; scalar requirements appear in
Properties and Layout, and `IntegerScalarCapacityComparison` applies the same durable scalar to
Comparison variants A/B so capacity, exact required codes, minimum-width thresholds, and fit
transitions remain headlessly testable. The shared derivation also reports the maximum session
target capacity permitted by the current code space after terminal-count and sentinel states, plus
remaining code-space headroom when the current requirement fits. It separately derives the capacity
limits implied by a known zero-based semantic range and the lowest concrete sentinel placement.
Only when all three constraints are known does it publish an effective valid-capacity limit and
headroom. Packed fields without a declared range therefore keep the effective result Unknown rather
than treating the code-space boundary as semantic fact. An `offset_into` relationship must declare
`:unit elements` or `:unit bytes`. Element offsets model `0..capacity-1`; byte offsets model
`0..total_allocation_bytes-1`, where total allocation bytes come from the selected session SoA
allocation strategy and remain Unknown when physical facts are incomplete. Graph labels retain the
unit, and both packed/scalar A/B analysis uses each variant's independently derived extent. These
target facts never enter the `TypeGraph`, never rewrite durable auto widths, and are not serialized
to LispB; only the relationship kind/unit/target are durable.

For standard-library SoAs, page footprints are calculated per column because each vector is a
separate allocation. The displayed aggregate is the sum of those minimum per-column page counts;
it does not assume shared pages or include allocator overhead.

Strong aliases, arbitrary ABI probing, persistent plans, chunking/AoSoA, arena planning,
performance prediction, and live-process inspection remain incomplete. Planner semantic changes
write back through LispB rather than a parallel persistence format.

## Frontend behavior

The SDL window provides independently visible dockable Project / Schema, Layout, Graph, Properties,
Variants, Comparison, Source, and Diagnostics views. Visibility and dock placement persist, and a
reset restores a sensible default workspace. Source presents `EditableSchemaDocument` preview
results directly as affected-file and Updated/Original tabs; it owns no source copy and File >
Preview only reveals/focuses that view. Diagnostics composes existing load, document-operation, and
active analyzer messages without becoming a separate validation system. Graph is a navigation-only
pan/zoom view over the resolved `TypeGraph`; its deterministic columns and field-aware edge labels
do not duplicate semantic state or assume the graph is acyclic. The renderer follows recent
activity: it runs smoothly during interaction, reduces its rate while idle or unfocused, and waits
for events while minimized.

### Responsive layout policy

Every action and piece of explanatory text must remain reachable within its owning view when a dock
pane or window narrows. Do not extend a row with unconditional `SameLine()` calls or fixed control
widths that push later items beyond the visible content region. Reflow button groups into additional
rows, let inputs use the available width, and wrap prose. The Project / Schema view uses
`WrappingButtonRow` for module-local declaration and source actions; a button that cannot fit at full
width stays inside the pane and exposes its full label on hover. For content that cannot sensibly wrap,
provide an explicit scrollable region rather than silently clipping it. Review new and changed
views at narrow dock widths as well as their default size.

New interactive tables use `begin_editable_table`, `editable_table_column`, and
`editable_table_row_handle`. Fixed-width columns and horizontal scrolling keep neighbours from
shrinking when a column is widened; long tables also scroll vertically. The row handle selects from
any cell in an unselected row without placing a hit target over editable controls in the selected
row.
