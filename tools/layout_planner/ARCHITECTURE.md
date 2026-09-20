# Memory layout planner architecture

## Structure

The headless planner library lives in `native/layout/lib/`; its GoogleTest suite is in
`native/layout/tests/`. The optional SDL3 and Dear ImGui frontend lives in
`tools/layout_planner/app/`, with GUI panels in `app/gui/` and SDL-specific headers in
`app/platform/`.

```text
native/lispb            parser, validated schema, and shared semantic type graph
native/layout/lib       headless layout analysis, schema loading, and variants
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

For existing enums and integer scalars, preview/save reparses the declaration's owned source slice
and performs token-local property replacement when the declaration name and ordered child identities
are unchanged. The same property patcher covers linear quantization, integer varint, fixed-point,
sentinel optional, presence-bit optional, and packed-value declarations. Packed preservation checks
ordered field/reserved kinds and names plus field named-code identities, and patches existing
relationships only when their source shape is stable. This preserves comments, whitespace, and
unchanged token spelling without creating a second syntax or semantic model. Enum values additionally
derive row-owned source ranges from the reparsed declaration: stable named rows can be reordered or
removed with their leading/trailing comments, while inserted or duplicated rows use the canonical
single-row renderer. Packed fields and reserved regions use the same bounded-block mechanism keyed
by segment kind and name. Stable segments retain leading/trailing comments and token-local edits to
their types, widths, kinds, named codes, and relationships through reorder or neighboring
insertion/deletion; only new segments use the canonical single-segment renderer. Unreal projections,
ambiguous source shapes, and changed nested packed structure still fall back to the canonical
declaration renderer. Records and raw unions use bounded named member/alternative blocks so stable
children retain comments and token-local type/count edits across insertion, duplication, deletion,
and reorder; new children render canonically. Tagged unions use the same bounded named alternative
blocks for structural edits while patching tags and keeping discriminant/export properties outside
the ordered region. Other structural edits retain canonical fallback. Standard-library SoAs use a
bounded named-member region ending before functions, fixed layouts, and single-allocation forms.
Stable members can be structurally edited only after those advanced forms are proven semantically
unchanged; unsupported derived allocator/mutable-view surfaces take canonical fallback. Fixed-
layout changes are localized separately: an existing fixed form is replaced or removed at its
owned form range, and a newly enabled fixed form is inserted after custom functions and before
single-allocation output. This preserves unrelated declaration comments, member formatting, and
opaque custom function text while still rendering the changed fixed form canonically.

Declaration rename is a typed document transaction rather than text replacement. Supported enum,
integer-scalar, representation, packed-value, record, union, and ordinary SoA declarations retain
their `DeclarationId`, discover users from the resolved graph, repair the corresponding shared-
schema `TypeRef` fields to an unambiguous qualified spelling, resolve the whole candidate graph, and
rerender every affected top-level declaration. SoA rename also repairs module-local
`nested-schema`/`fixed-schema` identities. Explicit view, mask, fixed-storage, and allocation helper
names remain stable; implicit generated names follow the renamed declaration, and collisions roll
the transaction back. The owned SoA name token is patched locally so comments and custom function
text survive. Registered aliases remain rejected because the types registry does not yet
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
their explicit removal.

Deletion is likewise enforced by the shared editable document rather than only by the frontend.
Every typed delete rejects a declaration with resolved reverse users before mutating the manifest,
or one bound to a registered alias, then validates and re-resolves the remaining candidate graph.
For source-backed declarations the document retains the exact top-level source range as history
metadata: preview removes that range, undo restores its ownership and stable declaration identity,
redo removes it again, and successful save/reload clears the tombstone. The UI uses the same
destructive confirmation for source-backed and newly created declarations.

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
Nested SoA columns edit optional fixed-schema and nested-schema references through the same
whole-schema command path, and array selection clears those nested-only properties before
validation. The coordinated mask workflow enables or disables the generated mask type, field enum,
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
count; C++ lowering is one consumer and emits an ordinary struct plus `std::array` where needed.
Physical member offsets and padding remain target analysis, not durable semantic properties.
The record analyzer recursively derives nested-record facts, fixed-array extents, offsets, object
alignment, and internal/tail padding from the selected ABI profile. Unknown target facts stay
Unknown, while illegal by-value record cycles are rejected during semantic resolution.
Record create/replace/delete commands share the same stable declaration identity, validation
rollback, undo/redo, preview, and atomic save/reload path as enum, packed, and SoA declarations.
The inline type chooser derives valid local names and registered `@name` references from the shared
document/type graph and target primitive spellings from the ABI profile; it does not own a parallel
type registry.
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

Raw unions are separate shared `union-module` declarations with ordered semantic alternatives and
optional fixed counts. They lower to ordinary C++ unions, while one aggregate-cycle validator
rejects direct and mixed record/union by-value recursion. Target analysis chooses the maximum
alternative extent and alignment, rounds the object size, and reports tail padding plus each
alternative's union slack. Selected-count analysis assumes a contiguous, cache-line/page-aligned
array and scales total storage, tail padding, and each alternative's conditional slack with checked
arithmetic. Conditional slack does not imply a runtime tag distribution.

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

The planner supports enum value-domain analysis, packed storage type/bit-range analysis, and flat
standard-library SoA/vector-SoA payload analysis. A shared schema-layer domain engine derives enum
literal/implicit values, named- and count-sentinel code use, signedness, and minimum width. LispB's
optional `:bit-width` is a durable semantic constraint (`auto` when absent), while the existing
underlying type remains a separate C++ lowering choice. Per-value `:sentinel true` metadata is
durable and may represent multiple reserved semantic states without conflating them with allocated
storage waste.
Optional `:signed` is another semantic-domain constraint; when absent, signedness is inferred from
known values. Width derivation consumes the semantic signedness rather than the backing primitive.
Known width mismatches are rejected during semantic
edits; arbitrary initializer expressions remain explicitly unknown. Packed analysis reports overflow-safe aggregate
storage, semantic payload bits, explicit reserved bits, implicit unused bits, cache lines, and pages
at a selected element count. The built-in profile records CMake-supplied platform, architecture,
compiler, and build configuration identity independently from its physical-fact provenance. ABI
identity remains Unknown until a factual identifier is supplied. Compiler-derived size/alignment
facts identify their source, while the x86/x86-64 baseline memory facts separately identify the
source of the explicit 64-byte cache-line and 4 KiB page assumptions. SoA cache tiling consumes the
same profile fact rather than an analyzer constant. Missing
profile facts, unknown physical types, and nested SoAs produce diagnostics and Unknown results
rather than guesses.

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
optional prelude: no physical C++ type is fabricated.

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
The configured generated header remains empty: this declaration does not yet choose a standalone
ABI wrapper, packed placement, or C++ lowering policy.
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
declared.

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
the signed or unsigned extremes across their range and named codes; enum fields consume the shared enum
domain width. The resolved `PackedField` retains an auto-provenance flag alongside its concrete
width, so validation, layout, variants, visualization, and C++ lowering never need a magic numeric
sentinel for "auto".
Packed semantic fields may own one source-backed relationship with a declared target. Relationship
kinds use shared schema enums (`index_into`, `count_of`, `offset_into`, `discriminates`, `contains`,
`member_of`, `quantises`, `encoded_as`, and `references`), and the target resolves through the same
`TypeRef`/`TypeGraph` machinery as field types. Targets that resolve only to external C++ leaves are
rejected: a semantic edge must terminate at a declaration. The relationship adds a dependency and
field-aware graph label, while C++ lowering remains unchanged because the relationship is semantic
metadata rather than a second physical field. Capacity-derived width is intentionally deferred
until target declarations provide factual capacity.

For standard-library SoAs, page footprints are calculated per column because each vector is a
separate allocation. The displayed aggregate is the sum of those minimum per-column page counts;
it does not assume shared pages or include allocator overhead.

Strong aliases, arbitrary ABI probing, persistent plans, chunking/AoSoA, arena planning,
performance prediction, and live-process inspection remain incomplete. Planner semantic changes
write back through LispB rather than a parallel persistence format.

## Frontend behavior

The SDL window provides dockable Project / Schema, Layout, Graph, Properties, Variants, and
Comparison panels. Graph is a navigation-only pan/zoom view over the resolved `TypeGraph`; its
deterministic columns and field-aware edge labels do not duplicate semantic state or assume the
graph is acyclic. The renderer follows recent activity: it runs smoothly during interaction,
reduces its rate while idle or unfocused, and waits for events while minimized.
