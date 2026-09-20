# Memory layout planner roadmap

## Product direction

The planner is an authoring tool, not only a schema viewer. LispB S-expressions remain the
canonical source representation. The application edits a source-aware schema draft, resolves that
draft into the shared semantic type graph, and uses the graph for layout analysis and C++ code
generation.

```text
LispB source
    <-> parse / source-preserving serialization
Editable schema document
    -> resolve and validate
Semantic type graph
    -> layout analysis
    -> GUI visualization
    -> C++ code generation
```

The editable document belongs to the LispB/schema library. The GUI must not introduce planner-only
copies of enums, packed fields, columns, or future type declarations.

## Current milestone

The editable document/command foundation supports enum, standalone integer-scalar, linear-
quantized, integer-varint, fixed-point, optional-sentinel, optional-presence-bit representation,
packed-value, record, and standard-library
SoA authoring. Declarations
can be added to existing matching modules;
ordered values/fields/members/codes can be added, duplicated, deleted, reordered, and edited inline;
packed widths can also be changed through the bit diagram. Semantic edits support undo/redo,
affected-source preview, and validated save/reload. Supported declarations can be renamed through
graph-derived reference repair, and source-less declarations created in the active draft can be
deleted after reverse-user checks. Source-backed deletion now removes the exact owned top-level
source range through undoable document tombstones and rejects registered-alias users. Module
creation remains follow-up work. Existing enums, integer scalars, and simple
representation and packed-value declarations now preserve declaration-local comments, whitespace,
and unchanged token spelling for ordinary nonstructural property/value edits. Stable packed
field/reserved segments, named codes, and existing relationships are covered. Enum value insertion,
duplication, deletion, and reorder now preserve stable existing row blocks with their leading and
trailing comments while rendering only new rows canonically. Packed field/reserved insertion,
duplication, deletion, and reorder use the same bounded-block approach, keyed by segment kind and
name; stable nested named codes and relationships retain their source text while new segments are
rendered canonically. Record members and raw-union alternatives also preserve stable named row
blocks through insertion, duplication, deletion, and reorder, rendering only new children
canonically. Tagged-union alternatives now use the same stable named-block behavior while keeping
discriminant/export properties outside the ordered alternative region. Structural child edits for
other declaration kinds still use canonical rendering. Ordinary standard-library SoA members use a
bounded named-block region that stops before custom functions, fixed layouts, and single-allocation
forms. Stable members retain comments and token-local kind/type/property edits through structural
changes while advanced untouched forms are semantically verified; unsupported or ambiguous edits
retain canonical fallback.

Project Open, Save, validated Save As cloning, persisted recent-project history, and module-first
schema browsing are implemented. A native file picker and richer multi-target project selection are
later usability work; the current path dialogs deliberately keep the file workflow simple.

Target profiles now identify platform, architecture, ABI, compiler, and build configuration as
independently optional facts. The built-in compiler profile obtains the identity facts CMake can
state, leaves ABI Unknown, and keeps primitive and memory-fact provenance separate. Optional cache
capacities drive packed, record, and standard-library SoA aggregate working-set fit without guessed
built-in values. Packed analysis
also supports selectable element-count presets and custom counts, reporting
overflow-safe aggregate bytes, unused bits, minimum cache lines, and minimum pages. Cache-line and
page sizes now come from an explicit x86/x86-64 baseline profile with provenance; unknown profile
facts remain unknown rather than falling back to analyzer literals.
Packed declarations use ordered field-or-reserved segments. Named `(reserved ... :bits N)` regions
occupy durable physical positions, generate no value API, and are reported separately from semantic
payload and implicit trailing unused storage.
Packed values may now declare `:bit-order lsb-first|msb-first` and optional serialized
`:byte-order little|big`. Omitted bit order preserves the historical LSB-first allocation. MSB-first
allocation changes resolved bit ranges and generated numeric getters/setters, including mixed
reserved segments; byte order is retained and displayed as an external serialization fact but does
not silently alter the host integer ABI or imply an unimplemented codec. Creation, inline editing,
token-local source preservation, undo/redo, save/reload, analysis, and the bit diagram support both.
Packed fields may use `:kind signed` with an explicit or derived arbitrary width up to 64 bits.
Validation requires a matching signed semantic integer type; analysis reports the exact
two's-complement range, and lowering performs checked assignment plus safe sign extension, including
full-width minimum values.
Signed and unsigned packed fields may declare an inclusive `:minimum` / `:maximum` semantic range.
Validation and generated setters enforce it; analysis reports semantic value count, minimum direct-
encoding width, and unused field codes separately from allocated bit waste.
Packed range-constrained integer fields may also own ordered signed or unsigned named codes. Ordinary codes must be in
the live range, while `:sentinel true` codes must be outside it; generated constants, setters,
raw-value validation, analysis, inline authoring, and save/reload all preserve those roles without
lowering the field as a language enum.
Packed fields may declare `:bits auto`. Shared resolution derives signed/unsigned integer widths
from range/code facts and enum widths from the enum semantic domain, then retains both auto provenance and a
concrete physical width for analysis and lowering. Unknown domains are rejected rather than guessed.
Standard-library SoA analysis also reports per-column and aggregate minimum pages using the target
page size, treating each vector as a separate allocation and excluding unknown allocator overhead.

Enum declarations now support an optional durable semantic `:bit-width`; absence means auto width
derived from literal and implicit values. Individual values may be marked `:sentinel true`, including
multiple named sentinel states independently of the existing count sentinel. The shared domain
engine accounts for reserved sentinel code use, signed/unsigned ranges, fit, and unused semantic
codes independently of the target backing type.
Optional `:signed true` or `:signed false` constrains the semantic domain; absence infers signedness
from known values. Signedness participates in minimum-width derivation independently of the C++
lowering type.
Known undersized domains are rejected through normal edit rollback; general initializer expressions
remain explicitly unknown. The existing underlying type remains the current C++ lowering preference,
not the semantic width.

Standalone `scalar-module` declarations now provide ordinary `integer-scalar` semantic domains
without a C++ underlying type. Each scalar stores signedness, an inclusive live range, auto or
explicit 1-64-bit width, and ordered named live/sentinel codes. The shared graph resolves these as
`IntegerScalarType` nodes with no target size/alignment facts; headless analysis reports live,
sentinel, required, and unused code space. Creation, flat inline editing and drag reorder,
undo/redo, preview, save, and reload use the same editable-document path. Scalar modules emit an
otherwise empty configured header and do not fabricate a physical C++ value type.

`representation-module` now provides an initial source-backed `linear-quantized` physical
representation. It references an `integer-scalar` through the shared graph rather than copying its
semantic range, owns an encoded width, reserved-code count, and reject/clamp clipping policy, and
deliberately has no standalone ABI `sizeof`. Exact capacity validation retains at least two usable
codes. Headless analysis reports source span, total/usable/reserved code space, linear resolution,
maximum rounding error, and exact endpoint mapping, including honest `2^64` handling. Creation and
inline editing use normal commands, undo/redo, preview, save, and reload. Signed source domains are
first-class: cross-zero, wholly negative, and full signed 64-bit ranges use the same checked span,
analysis, comparison, and source-backed authoring paths as unsigned domains.
The Comparison view can select any sibling linear quantization of the same semantic source and uses
headless comparison analysis for code-space, precision/error, clipping, and overflow-safe
selected-count payload-bit deltas. Allocated bytes/cache/page consequences remain Unknown until a
container placement policy is declared.

`representation-module` also supports source-backed `integer-varint` declarations for unsigned,
signed, and ZigZag encodings. Signedness compatibility is validated against the referenced integer
scalar; named source sentinels participate in exact minimum/maximum encoded byte analysis.
Selected-count lower/upper byte bounds are overflow-safe. An explicit session value/weight
distribution enables checked sample totals and a clearly distribution-based expected size while
keeping workload frequencies out of LispB. Per-value and weighted-byte breakdowns remain available
even when an exact aggregate overflows. Creation, flat source/encoding editing, undo/redo, preview,
save/reload, graph navigation, and end-to-end analysis are implemented without a fake fixed
`sizeof`.
Same-source varint declarations can also be compared directly for encoding policy and exact
per-value/selected-count lower and upper byte bounds. Different semantic sources are rejected and
expected size stays Unknown without a distribution. When supplied, one source-keyed session
distribution is applied to both encodings for checked sample totals and expectation deltas.

`representation-module` now also supports source-backed standalone `fixed-point` declarations.
Signedness, total width, fractional width, and nearest-even/toward-zero rounding resolve to a
first-class semantic representation without a fabricated C++ primitive or ABI `sizeof`. Headless
analysis reports exact raw range, whole/fractional/sign allocation, scale, resolution, numerical
range, rounding-error bound, and overflow-safe selected-count encoded bits. Creation, flat editing,
duplication, rename, draft deletion, undo/redo, preview, save/reload, browser/graph navigation, and
end-to-end analysis are implemented.

`representation-module` now supports source-backed `mini-float` representations. Explicit
sign, exponent, significand, and bias fields use an initial IEEE-style policy with distinct zero/
subnormal, normal, and infinity/NaN exponent roles. Resolution creates a first-class semantic node
without inventing a compiler primitive or ABI `sizeof`; analysis reports exact code roles and
exponent facts, numerical extrema where the host can represent them, rounding/resolution facts, and
overflow-safe selected-count payload bits. Typed creation/replacement/deletion, token-local source
preservation, duplication, rename, undo/redo, preview, save/reload, browser/graph navigation, and a
flat inline Properties editor are implemented.

`representation-module` also supports source-backed `optional-sentinel` declarations. Each one
references an integer scalar and selects one of its named sentinel codes as the absence state.
Validation rejects non-scalar sources plus missing or non-sentinel codes; resolution retains the
exact code, source-derived width, and dependency. Analysis separates present values, the absence
code, other sentinels, and unused codes, and scales encoded payload bits without inventing ABI
storage. Typed history/source persistence, scalar-rename repair, creation, duplication/deletion,
flat constrained source/sentinel editing, graph navigation, and Layout/Properties presentation are
implemented.

`representation-module` also supports source-backed `optional-presence-bit` declarations as a
distinct sibling policy over an integer scalar. Resolution derives one presence bit plus the source
payload width, including 65 encoded bits for a 64-bit source. Analysis separates one canonical
absence state from redundant absent payload patterns, source sentinel/unused code space, and
overflow-safe selected-count encoded bits without inventing ABI placement. Typed history/source
persistence, scalar-rename repair, creation, duplication/deletion, flat source editing, graph
navigation, and Layout/Properties presentation are implemented. Byte packing and bit order remain
future placement facts. Any two same-source sentinel/presence declarations can be compared through
headless analysis and the Comparison view; policy roles, encoded width, redundant absence patterns,
and selected-count payload-bit deltas remain separate from unspecified allocated storage.

The shared LispB schema now has an initial ordinary `record-module`: records contain semantic
members and optional fixed element counts, resolve to `RecordType` nodes and dependency edges, and
lower to dependency-ordered ordinary C++ structs with `std::array` for fixed arrays. Illegal
by-value record cycles are rejected. Target-derived record analysis reports recursive member
offsets, fixed-array extents, alignment, internal/tail padding, and total size in a byte map while
keeping unknown facts explicit. Record declarations are now source-backed and editable through a
flat member grid with add/duplicate/delete/reorder, scalar/fixed-array cardinality, preview,
undo/redo, and save/reload. Configurable-count record totals now cover storage, member extents,
internal/tail padding, minimum cache lines, and minimum pages with overflow-safe arithmetic. A
shared type chooser supports local, registered, and target-physical references. Cache-line/page boundary
crossing counts use an explicit contiguous, region-aligned array model and avoid performance claims.
Packed, record, and SoA inline type cells now share a searchable chooser for valid local,
registered, and target-physical references plus navigation to the resolved semantic type.
The record grid now defines an explicit session-only multi-member sequential access set, defaulting
to the focused member, and reports unioned useful bytes, enclosing AoS footprint, distinct cache
lines/pages, and non-selected bytes in touched cache lines under the documented aligned-array model.
Packed fields now support optional source-backed semantic relationships to declared types. Initial
kinds cover index/count/offset, discriminant, containment/membership, quantisation/encoding, and
general reference edges. They round-trip, validate, appear as labeled graph dependencies, and are
editable/navigable inline. Capacity propagation remains pending durable table/buffer capacity facts.

The shared schema also has a source-backed `union-module` / raw `union` vertical slice. Ordered
alternatives may be scalar or fixed arrays, resolve to first-class dependency-bearing semantic
nodes, reject direct and mixed record/union by-value cycles, and lower to ordinary C++ unions.
Stable-ID commands, preview/save/reload, creation, duplication/deletion, and the flat alternative
grid cover name/type/count editing, type navigation, and button/drag reorder. Target analysis
derives maximum alternative extent, alignment-rounded size, tail padding, and per-alternative union
slack while leaving unknown/overflowed facts explicit. Selected-count analysis scales storage,
tail padding, and conditional per-alternative slack, and reports target-driven cache-line/page
footprints and boundary crossings without inventing an alternative distribution.

The initial tagged-union foundation is also source-backed. A distinct `tagged-union` declaration
references an enum discriminant and gives each ordered payload alternative one symbolic,
non-sentinel tag. Parsing, semantic validation, dependency edges, mixed aggregate-cycle rejection,
and dependency-ordered C++ lowering are implemented. Target analysis reflects the explicit
tag-plus-payload lowering and reports discriminant facts, payload offset/size/alignment, inter-member
and tail padding, total object facts, and conditional payload slack. Layout and Properties expose
those facts with a tag/padding/payload object map. Typed create/replace/delete commands, stable
identity/history, canonical preview/save/reload, rename repair, exact source deletion, declaration
duplication, creation UI, and the flat alternative editor are implemented. The editor covers
discriminant choice, unique non-sentinel tags, type navigation, optional fixed counts,
add/duplicate/delete, and button/drag reorder without planner-owned semantic copies.
Configurable-count tagged analysis now scales object/discriminant/payload/padding and conditional
per-tag slack with overflow-safe arithmetic, reports target cache/page footprints and exact aligned-
array boundary crossings, and consumes optional cache capacities without inventing tag frequencies.
Symbolic discriminant coverage separately identifies payload-mapped live tags, unmapped live tags,
named sentinels, and the count sentinel in Layout/Properties; these semantic roles are not presented
as allocated padding or confused with undeclared numeric enum codes.
Explicit session tag weights now drive headless workload analysis of active payload extent and
conditional slack. Per-row checked weighted facts, checked sample totals, and expected per-value/
selected-count values are shown in Properties. Invalid roles, duplicates, unknown facts, zero total
weight, and overflow remain diagnostic, and weights never enter LispB source.

## 1. Editable document and command foundation

- Own a mutable draft of the validated LispB declarations.
- Retain source-file ownership and source ranges for declarations.
- Assign stable declaration IDs that survive ordinary edits.
- Apply typed semantic commands rather than text substitutions.
- Re-resolve and validate the semantic graph after each command.
- Keep invalid operations from corrupting the last valid draft.
- Provide undo/redo, dirty state, and a monotonic revision for consumers.
- Track the saved history position independently from the edit revision.
- Later extend this foundation with precise serialization, atomic save, diff preview, and
  reparse-after-save verification.

## 2. Enum authoring vertical slice

- Create an enum in a selected module and namespace.
- Select its underlying semantic type.
- Add, remove, duplicate, and reorder enumerators.
- Edit symbolic names, explicit values, display names, serialized names, hidden markers, named
  sentinel roles, and count-sentinel semantics.
- Rename and delete safely with dependency-aware diagnostics.
- Save the declaration to LispB, reload it, resolve the same semantic graph, and generate the
  expected C++.

## 3. Packed-value and bit-field authoring

The initial end-to-end slice, including explicit reserved-bit regions and named field sentinel
codes, is implemented. Reusable standalone named-value/range-constrained integer scalars and an
initial explicit linear-quantized representation are implemented separately; packed/container
placement and machine API lowering remain future work.

- Create packed values with a backing storage type and optional invalid raw value.
- Add, remove, duplicate, and reorder fields.
- Select each field's logical semantic type.
- Edit bit width, packed field kind, and existing range-helper metadata.
- Choose explicit or semantically derived (`:bits auto`) field width.
- Constrain signed or unsigned field semantics with durable inclusive minimum/maximum bounds.
- Add, edit, duplicate, delete, and reorder named ordinary and out-of-range sentinel codes.
- Add, edit, duplicate, delete, and reorder named reserved-bit regions without inventing semantic
  field types or generated accessors.
- Support both numeric edits and direct divider dragging.
- Display unused bits, overflow, representable ranges, and dependency edges immediately.
- Allow creation of a referenced enum from the field workflow.
- Save and reload the resulting LispB without losing semantics.

The primary acceptance case is creating an enum backed by `uint8`, then creating a packed `uint32`
value with a 24-bit integer field and an 8-bit field that semantically references that enum.

## 4. SoA authoring

The initial standard-library SoA vertical slice is implemented. Selected nested columns expose
optional fixed-schema and nested-schema references inline, and switching back to array clears those
nested-only properties. Coordinated generated field-mask/storage/dimension authoring remains
partially complete: the UI atomically enables generated names, the required storage column, and the
first selected mask field; it safely disables the whole configuration and toggles additional
eligible array fields without allowing the final field to be removed. Selected mask fields expose
an inline ordered dimension table with add, duplicate, delete, button-and-drag reorder, and direct
name/extent editing through normal semantic commands. Fixed layouts can be enabled or disabled
inline with a collision-free storage type; storage and ordered container names support direct
editing plus add/duplicate/delete/button-and-drag reorder through the same document commands.
Mutable and const view type names
independently switch between visible derived defaults and directly editable explicit names. Single-
allocation output can be enabled with a collision-free owner/Storage pair, renamed inline, and
disabled when it has no allocator variants; owner edits retain existing variants unchanged. A flat
allocator-variant table supports staged add, direct owner/allocator edits, shared type picking,
duplicate/delete, button reorder, and drag reorder with paired generated-name collision avoidance.
The complete storage-operation capability set is exposed as inline source-named toggles with
enable-all/disable-all actions; changes are normalized to shared canonical order and use the same
undoable document commands.
Generation policy is authorable inline: optional export and equivalent-row types, layout-only
emission, and memberwise copy behavior all remain source-backed. Equivalent-row types are resolved
semantic graph dependencies, appear as labeled graph edges, participate in rename/deletion safety,
and support type picking plus direct navigation.
Opaque generated `using` declaration fragments have a flat ordered editor with add, direct edit,
delete, button reorder, and drag reorder. They remain source-backed escaped strings rather than
being misrepresented as semantic graph relationships.
Custom storage functions now have an initial flat lifecycle/signature editor: add, duplicate,
delete, button/drag reorder, name, return type, const/noexcept/static qualifiers, and inline/source
placement all submit ordinary SoA document commands. The selected function's parameters have a flat
ordered table with add, metadata-preserving duplicate, delete, button/drag reorder, direct name/type
editing, shared type picking, and optional default expressions. Shared validation enforces nonblank
defaults and C++ trailing-default order before a document command is accepted. Bodies, dependencies,
trailing returns, templates, and constraints remain visibly inspectable and are preserved exactly by
these edits pending their own focused editors. Stable uniquely named function and parameter rows
preserve surrounding source comments and unchanged body text through structural edits; ambiguous
overloads and raw-body forms retain safe canonical fallback.

- Create standard-library SoA declarations.
- Add, remove, duplicate, and reorder columns.
- Select semantic column types and supported column kinds.
- Navigate to or create referenced types.
- Keep planning-only capacity experiments separate from source semantics unless capacity becomes a
  declared LispB property.
- Save, reload, analyze, and generate the declaration.

## 5. Relationship and declaration lifecycle operations

An initial dockable pan/zoom relationship graph is implemented over the resolved semantic graph,
with selectable nodes and field-aware dependency labels. Packed-field relationship editing and the
initial shared relationship-kind set are implemented. Duplication, supported-kind rename, and
reverse-user/registered-alias-safe deletion of draft or source-backed declarations are implemented;
declaration move and capacity-derived consequences remain future work.

- Provide a searchable semantic type picker.
- Navigate from a field or column to its referenced definition.
- Rename declarations while updating semantic references deliberately.
- Report all reverse users before deletion.
- Reject unsafe deletion or require explicit reference repair.
- Duplicate declarations under a new stable identity.
- Move declarations between modules where legal.
- Keep unresolved references as explicit draft errors; never silently convert them to external
  leaves.

Nonmodal sibling duplication is implemented for all currently editable declaration kinds through
their typed create commands. Standard-library SoAs use shared document-level copy preparation that
reserves a collision-free generated namespace for explicit views, field mask/enum types, fixed
storage/containers, single-allocation owners, and allocator variants. The copied field-mask storage
member is repaired to the new generated type before the normal `CreateSoa` command validates and
records the edit.
Typed rename is implemented for enums, integer scalars, quantizations, varints, packed values,
records, unions, and ordinary SoA declarations with stable declaration ID, graph-derived reverse-
user repair, undo/redo, multi-declaration preview, and save/reload. SoA rename additionally repairs
module-local nested/fixed schema identities, retains explicit helper names while implicit names
follow the declaration, and validates generated-name collisions transactionally. Registered-alias
repair and vector-SoA module rename remain lifecycle extensions.
Draft and source-backed declarations can be deleted through their existing typed commands only when
they have no resolved reverse users or registered alias. The document performs those checks before
mutation, validates the remaining manifest, and preserves undo/redo and stable identity on
restoration. Exact source tombstones remove only the declaration's owned top-level range in preview
and save; successful reload discards stale tombstone metadata.

## 6. Semantic design variants

Evolve variants from physical override maps into typed change sets over the baseline document.
Variants may add, remove, rename, or structurally edit declarations and relationships. Comparison
should report both semantic changes and their physical consequences. Recovery state may preserve
draft work, but accepted durable changes are written to LispB rather than a competing schema
format.

An initial declared-representation comparison is implemented for same-source linear quantizations;
it compares durable sibling declarations directly rather than forcing them into the session-only
physical override map. General typed semantic change sets remain future work.

## 7. Struct and AoS synthesis

Extend the shared type graph and authoring document with structures, typed members, fixed arrays,
alignment, offsets, internal padding, and tail padding. Support member-order experiments and
hot/cold grouping using the same command, validation, serialization, and dependency mechanisms.

## 8. Workload and system-level planning

- Compose semantic types into explicit planning scenarios.
- Model counts, capacities, multiplicity, allocation strategies, and selected access sets.
- Report payload, padding, capacity slack, allocation count, cache lines, pages, and total memory.
- Compare complete subsystem costs and identify the largest contributors.
- Reference semantic types by stable identity rather than redeclaring their schemas.

## 9. Physical-fact accuracy

- Record provenance for every ABI fact. Primitive and memory fact groups now carry provenance;
  generated per-fact loading remains follow-up work.
- Load generated `sizeof` and `alignof` facts from the real target compiler and configuration.
- Identify profiles by platform, architecture, ABI, compiler, and build configuration. The explicit
  optional identity value and honest Unknown handling are implemented.
- Accept optional L1-data/L2/L3 capacity facts and compare known aggregate working sets against
  them. This is implemented without assigning model-specific capacities to the baseline profile.
- Continue reporting unknown facts instead of guessing.
- Model allocator and single-allocation details only when the selected backend supplies factual
  rules.

## 10. Advanced layouts

After the authoring and analysis loop is reliable, extend the semantic graph with unions, tagged
unions, arrays, AoSoA/chunking, handles, references, containers, and arena/block allocation.
Performance claims should be based on explicit access patterns or measurements rather than a
speculative aggregate score.

## Validation strategy

Keep authoring and analysis behavior testable without ImGui. Cover command inversion, undo/redo,
stable identity, duplicate and unresolved references, source ownership, serialization round trips,
ABI boundary cases, overflow-safe aggregates, and unchanged generated C++ for unedited schemas.
Each new declaration kind should have an end-to-end create, save, reload, resolve, analyze, and
generate acceptance case.
