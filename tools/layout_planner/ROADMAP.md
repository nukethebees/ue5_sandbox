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

## V1 convergence and release boundary (2026-09-21)

The active V1 work is complete; do not read the ambitious sections below as an open V1 checklist.
The authoritative local session plan classifies every remaining historical TODO/IN PROGRESS item
as future/V2 unless a reproducible correctness defect is found in an exposed workflow. The final
contextual packed mini-float authoring slice is complete. No new feature slice follows it.

V1 supports LispB project open/recent/reopen, validated project cloning and source registration;
shared source-backed enum, integer scalar, representation, packed value, record, union, tagged
union, and standard-library SoA editing; inline ordered authoring and drag reorder; command-based
undo/redo; source preview, diagnostics, dirty-close protection, and validated atomic save/reload.
Packed linear-quantized, fixed-point, and mini-float placements retain shared representation
identity; generated APIs deliberately expose encoded/raw bits rather than fabricated decoded
semantics. Existing layout, waste, cache/page, selected-access, comparison, graph, and docked views
report only facts established by explicit target/session inputs, showing Unknown otherwise.

Known V1 limits: some complex structural edits safely canonicalize their declaration; ordinary
Open/Save As still use paths rather than native pickers; variants and capacity/workloads are
session-only planning state, while accepted semantic edits persist through LispB; target facts
focus on explicit x86/x86-64 profiles; a standalone mini-float does not imply a C++ floating ABI
or conversion arithmetic. These are honest scope boundaries, not known source corruption or
unfinished exposed UI.

Future/V2 only: additional representation placement combinations or contextual shortcuts,
strong aliases, new encoding families, UNORM/SNORM and custom codecs, optional/union physical
policies, durable typed semantic variants/scenarios, richer workload/page/zoom analyses, new
target architectures, sophisticated automatic graph layout, allocator or measured-performance
models, native file pickers, and further token-level source preservation beyond safe fallback.
The numbered design sections below retain this history without implicitly requiring its
completion for V1.

## Current milestone

The editable document/command foundation supports enum, standalone integer-scalar, linear-
quantized, integer-varint, fixed-point, mini-float, optional-sentinel, optional-presence-bit
representation, packed-value, record, and standard-library SoA authoring. Declarations
can coexist in ordinary modules, and empty modules can be created with an explicit SoA
backend in an already loaded module source;
ordered values/fields/members/codes can be added, duplicated, deleted, reordered, and edited inline;
packed widths can also be changed through the bit diagram. Semantic edits support undo/redo,
affected-source preview, and validated save/reload. Supported declarations can be renamed through
graph-derived reference repair, and source-less declarations created in the active draft can be
deleted after reverse-user checks. Source-backed deletion now removes the exact owned top-level
source range through undoable document tombstones and rejects registered-alias users. A shared
source-aware editable project document can register or unregister an existing valid `cpp-schema`
source, or stage a brand-new relative source, with typed undo/redo and target-wide validation.
Unregistration never deletes the source file. Preview includes the
exact manifest edit and complete new-file contents; save validates temporary files together,
publishes new sources before atomically replacing the manifest, rolls them back if manifest
publication fails, and reloads clean state. The Project view lists the live registry, unregisters
sources, registers an existing relative source, or stages an empty one nonmodally; global preview,
Undo/Redo, Save, project switching, and
dirty-close protection include
the project draft. Schema and project histories are intentionally exclusive, and successful Save
reloads the schema so the empty source can immediately receive its first module through the
ordinary editable-schema command path. Editable
declarations can move between compatible existing modules while retaining stable IDs, exact owned
source, undo/redo, and validated cross-file save/reload. Cross-namespace moves repair supported
graph-derived references atomically and reject registered aliases or module-local SoA links that
cannot be represented safely. Existing enums,
integer scalars, and simple
representation and packed-value declarations now preserve declaration-local comments, whitespace,
and unchanged token spelling for ordinary nonstructural property/value edits. Stable packed
field/reserved segments, named codes, and relationship add/edit/clear forms are covered for packed
fields and standalone integer scalars. Enum value insertion,
duplication, deletion, and reorder now preserve stable existing row blocks with their leading and
trailing comments while rendering only new rows canonically. One unambiguous same-position direct
enumerator rename now preserves that exact row block and patches only the owned name token;
multi-row or rename/reorder ambiguity remains conservative. Standalone integer-scalar code rows
now use the same stable-name block behavior, including one unambiguous same-position direct rename,
while scalar properties and relationships stay outside
the ordered region. Packed field/reserved insertion, duplication, deletion, and reorder use the
same bounded-block approach, keyed by segment kind and name; stable nested named codes and
relationships retain their source text while new segments are rendered canonically. Nested packed
named-code rows now also preserve stable blocks through insertion, duplication, deletion, and
reorder. One unambiguous same-position direct rename patches the original row's name token; new or
ambiguous names render canonically. One unambiguous same-position packed field or reserved-region
rename likewise retains its complete block and patches only the atomic name when all non-name
semantics match; mixed-kind, rename-plus-edit, and multi-row ambiguity stay conservative. Empty
scalar/field code regions support local
first-row insertion before a relationship or parent close and deletion back to empty. Record
members and raw-union alternatives also
preserve stable named row
blocks through insertion, duplication, deletion, and reorder, rendering only new children
canonically. One unambiguous same-position direct child rename retains its exact row block and
patches the name token. Tagged-union alternatives use the same behavior while keeping
discriminant/export properties outside the ordered alternative region. Structural child edits for
other declaration kinds still use canonical rendering. Ordinary standard-library SoA members use a
bounded named-block region that stops before custom functions, fixed layouts, and single-allocation
forms. Stable members retain comments and token-local kind/type/property edits through structural
changes; one same-position direct member rename retains its block only when every non-name semantic
matches. Advanced untouched forms are semantically verified; unsupported or ambiguous edits
retain canonical fallback. Existing single-allocation forms now patch their owner locally and
preserve stable allocator-variant row blocks through allocator edits, insertion, deletion, reorder,
and one unambiguous allocator-preserving direct rename; whole-form enable/disable remains canonical.
Existing fixed-layout forms now patch their storage name and ordered container region locally;
multiline/commented lists preserve stable blocks through structural edits and one unambiguous direct
rename, while compact lists avoid whole-declaration canonicalization.
Existing member mask-dimension lists now preserve stable named dimension blocks and comments through
extent edits, insertion, deletion, reorder, and one extent-preserving direct rename; first add/last
remove remain localized canonical property operations.
Multiline `using` declaration lists now preserve unique opaque-string blocks and comments through
insertion, deletion, reorder, and one direct value edit; compact or ambiguous/duplicate lists
canonicalize only the property value rather than the whole declaration.
Custom-function list-form body fragments and registered dependency keys now use the same unique
quoted-row boundary. Stable multiline blocks preserve comments and escaping through insertion,
deletion, reorder, and one direct edit; compact or ambiguous/duplicate lists retain localized
fallback. Unchanged raw `#cpp` bodies remain exact, while a semantic body edit replaces only the
bounded body property value with the quoted-list form.
Multiline SoA storage-operation lists use the corresponding atom-row boundary. Stable unique
operation blocks preserve their comments and spacing through capability insertion, deletion,
reorder, and one direct substitution; compact or ambiguous lists canonicalize only the bounded
property value, and first-add/last-remove remain ordinary localized property edits. An unchanged
`(all)` shorthand remains exact and expands only when the resolved capability set changes.
Enum conversion policy is now authorable as a flat inline checkbox set backed directly by the
shared `EnumSchema`. Shared descriptors provide one durable order and source spelling. Multiline
`:conversions` lists use the same atom-row source boundary, retaining stable unique rows and
comments through insertion, deletion, reorder, and direct substitution while compact or ambiguous
lists keep fallback localized to the property value.
The remaining ordinary enum generation policy is inline as well: reflection mode, enum-array
helpers, native API mode, and the optional export specifier all use `ReplaceEnum`. Shared
validation rejects incompatible native/Unreal combinations transactionally, and ordinary property
patching retains declaration comments and unrelated value/conversion rows through save/reload.
Optional export specifiers are also directly authorable for packed values, records, raw unions, and
tagged unions. Each flat input uses its declaration's typed replace command and shared identifier
validation, while existing property source boundaries retain comments and ordered child rows.
Packed values additionally expose the existing mutable generated-API policy inline through
`ReplacePackedValue`; it remains separate from semantic domain and physical layout facts.
Native plain enums now expose their complete optional Unreal projection inline, including every
required generated path/include and projection reflection mode. The nested source boundary patches
existing forms locally and bounds add/remove to the form, retaining enum comments and value rows.
Selected packed fields can now create a shared enum with their resolved width prefilled and bind it
through the existing enum-create and packed-replace histories. The same shared type chooser exposes
unique cross-module declarations using qualified source spellings and marks ambiguous spellings
nonselectable rather than allowing them to resolve as external leaves. Choosing an existing enum
updates the packed field type and kind together. A rejected contextual bind rolls back the new enum.
Signed/unsigned packed fields can likewise create a shared integer scalar with the current range,
signedness, width policy, and named codes prefilled, then bind through the existing scalar-create
and packed-replace histories. Placement width and field relationship remain intact, two-step
undo/redo is preserved, and a rejected bind rolls the new scalar back.
Existing linear-quantized representations are selectable as packed field types through an explicit
`linear-quantized` kind. Auto width follows the representation exactly, competing field-local domain
facts are rejected, packed analysis reuses the shared quantization facts, and generated APIs expose
validated raw encoded codes with explicit names instead of fabricating a decoded semantic type.
Ordinary standard-library SoA array columns now offer the corresponding shared-record workflow. The
record is initialized with the column's current semantic type, then the existing `CreateRecord` and
`ReplaceSoa` histories bind it using qualified source spelling. Column kind, generation metadata,
and session planning state remain unchanged; failed binding rolls the new record back.
Packed signed and unsigned fields can now reference standalone integer-scalar declarations as their
semantic domain. Validation derives auto width and fit from the scalar's signed range, explicit or
derived semantic width, and named sentinel codes while rejecting competing field-local domains.
The graph retains the scalar dependency and publishes its range/code facts on the resolved field for
analysis. C++ lowering chooses a conventional fixed-width accessor only for the packed placement,
preserving the scalar's lack of standalone ABI facts. The type picker performs the kind/domain
transition atomically and the packed editor presents the shared range and codes as scalar-owned.

Project Open, Save, validated Save As cloning, persisted recent-project history, and module-first
schema browsing are implemented. A native file picker and richer multi-target project selection are
later usability work; the current path dialogs deliberately keep the file workflow simple.
All major workbench views are independently showable, dockable, and visibility-persistent. Source
is a nonmodal affected-file Updated/Original inspector over the editable document preview, and
Diagnostics consolidates existing project/document/active-analysis messages. Reset restores the
default workspace rather than requiring manual reconstruction.

Target profiles now identify platform, architecture, ABI, compiler, and build configuration as
independently optional facts. The built-in compiler profile obtains the identity facts CMake can
state, leaves ABI Unknown, and keeps primitive and memory-fact provenance separate. Optional cache
capacities drive packed, record, and standard-library SoA aggregate working-set fit without guessed
built-in values. Layout exposes cache-line, page, and optional cache-capacity facts as session-only
target-profile inputs; applying or restoring them immediately refreshes analysis without changing
LispB or semantic dirty history. A target-built `layout-profile-probe` now serializes the exact
compiler/configuration primitive facts to a deterministic versioned text profile. The planner can
load that profile explicitly for the session; parsing rejects malformed sizes, alignments, integer
metadata, duplicates, and representation cycles transactionally while omitted facts remain
Unknown. Individual primitive/alias facts are inspectable, and the selected path persists per
project through UI workspace settings; stale files fall back to the built-in profile with a
diagnostic rather than contaminating schema state. For a selected record, Comparison accepts a
second session-only target profile and applies both profiles to the same semantic record and element
count. Headless name-based member matching reports checked member/object/aggregate/cache/page deltas,
keeps Unknown facts unknown, prefixes diagnostics by side, and rejects mismatched semantic inputs
without partial results. The selected-member intent map and multiplicity are also held fixed across
both record targets; a checked workload join reports useful/logical bytes, enclosing AoS footprint,
exact read/write cache/page address coverage, non-selected footprint, and cache fit without claiming
measured traffic. Raw unions reuse the same selected-count/profile workflow, transactionally
matching alternatives before reporting element size/alignment, extent, conditional slack, object,
aggregate, cache/page, straddling, and cache-fit consequences without making an ABI-compatibility
claim. Tagged unions extend this with transactional discriminant/tag-role validation and report
discriminant, payload-union, object-padding, alternative-slack, and aggregate physical changes while
applying any optional explicit distribution's exact same tag weights to both targets for checked
per-entry, weighted, expected-per-value, and selected-count extent/slack deltas. Selected packed values
reuse that profile B while holding the active
physical variant fixed; transactional ordered-segment matching exposes target storage, bit
position/overflow, relationship-capacity, aggregate, cache-line, and page differences separately
from the existing variant comparison. The complete selected-field intent map and multiplicity are
also held fixed across packed targets, exposing useful/logical bits, containing storage, waste
categories, minimum read/write cache/page coverage, and cache fit without implying sub-word fetches.
Standard-library SoAs also hold the active variant, capacity,
allocation strategy, and selected workload fixed across profiles. Transactional ordered-column
matching reports target-dependent size/alignment, contiguous offsets/padding, allocation,
cache/page, cache-fit, workload coverage, straddling, and capacity-slack facts before the existing
same-target variant comparison. The UI makes no performance claim and neither profile enters LispB.
Selected enums also reuse profile B while holding their complete semantic domain, ordered codes,
sentinel roles, signedness, semantic width, and C++ backing spelling fixed. A transactional join
reports target backing size/alignment/storage bits/value-bit capacity/domain fit/code slack without
giving semantic width a fabricated standalone size or turning code-space slack into byte waste.
The shared selected count separately scales the generated standalone backing into checked storage,
cache/page, aligned-origin straddling, and cache-fit facts under both profiles; it does not imply
bit-packed enum storage.
Packed analysis
also supports selectable element-count presets and custom counts, reporting
overflow-safe aggregate bytes, unused bits, minimum cache lines/pages, and aligned-contiguous
cache-line/page boundary-straddling element counts. Cache-line and page sizes now come from an
explicit x86/x86-64 baseline profile with provenance; unknown profile facts remain unknown rather
than falling back to analyzer literals.
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
from local range/code facts or a referenced integer scalar, and enum widths from the enum semantic
domain, then retains both auto provenance and a concrete physical width for analysis and lowering.
Unknown domains are rejected rather than guessed.
Under the default separate-column strategy, standard-library SoA analysis reports per-column and
aggregate minimum pages using the target page size, treating each vector as an independent
allocation and excluding unknown allocator overhead.
An explicit session-only SoA access set now reports useful and unselected logical payload at the
shared element count, allocated payload at the independently modeled capacity, and the sum of each
selected column's minimum cache-line/page footprint. It also reports unused allocated-capacity
payload separately from unselected-column payload at the workload count. Flat Access checkboxes plus
selected-only/all actions drive the analysis; unknown target facts and arithmetic overflow remain
Unknown, and the UI labels region bytes as minimum physical footprints rather than measured traffic
or performance.
A packed-field access set uses the same flat selection and per-field read/write intent. Headless
analysis reports selected physical and multiplicity-scaled logical useful bits, whole-storage
footprint, non-useful storage bits, minimum cache-line/page address coverage, operation-specific
coverage, and cache-capacity fit. Selecting fewer bitfields never fabricates a sub-word physical
fetch; Unknown target facts and overflow stay explicit.
The packed Comparison view applies one complete field-intent map, count, and multiplicity to both
selected physical variants. Checked deltas separate selected/useful-bit changes from containing-word
storage, cache-line, page, and cache-fit consequences. Mismatched workloads are rejected; unknown or
overflowed physical facts stay Unknown, and the UI makes no ranking or performance claim. An
expandable per-field breakdown retains operation, physical width, classified useful bits, and
multiplicity-scaled logical bits for each selected field; it deliberately does not invent per-field
cache/page fetch coverage. Aggregate non-useful storage is decomposed into checked unselected-field,
explicit-reserved-region, and physically-unused-backing categories whose sum is verified when all
facts are known. Overflowed or invalid physical layouts leave those categories Unknown.
The Comparison view now applies that same selected-column workload to both selected physical
variants. Each side retains its own capacity and column type overrides while headless comparison
reports useful payload, minimum cache-line/page footprints, allocated payload at capacity, capacity
slack, and checked deltas. An expandable per-column breakdown identifies the exact physical type,
element size, footprint, capacity, and slack contribution behind each aggregate delta. Mismatched
workloads are rejected. Complete logical payload, unselected workload payload, capacity slack, and
non-payload bytes inside minimum cache/page footprints remain distinct comparison categories. The
selected access set's minimum cache-line footprint is also compared against optional target L1,
L2, and L3 capacities. Equivalent-record comparison retains the record's exact aligned-array fit
beside the SoA lower-bound fit, and separately reports useful versus non-useful bytes in each
cache/page footprint. Selected columns additionally report aligned-origin cache-line/page
straddling counts, including checked cross-variant deltas. Each selected record member or SoA
column can be classified as read, write, or read+write while unique storage and address-coverage
facts stay separate from classified useful bytes. Comparisons require the same complete named
intent map. Positive access multiplicity scales overflow-checked logical useful-byte totals without
multiplying the physical footprint. Read and write cache/page address coverage is derived
separately—exact for aligned-contiguous AoS and as a separate-allocation minimum for SoA—without
inferring write policy or measured traffic. Compact composition bars visualize those categories
from analysis facts only;
the UI labels the models explicitly and makes no speed claim.

SoA allocation strategy is now explicit session-only analysis state. The existing separate-column
strategy retains one allocation per column and lower-bound region sums. The aligned-contiguous
strategy places capacity-sized columns in declaration order using target-derived alignment, reports
column offsets, padding, total block bytes, block alignment, and allocation count, and unions
selected prefixes into exact cache/page coverage from a region-aligned block origin. Both variants
in a comparison use the same chosen strategy. Capacity and strategy do not enter LispB, and
allocator headers, growth policy, heap overhead, and speed remain Unknown.
The aligned-contiguous Layout view also provides a horizontally scrollable cache-line/page map over
those analyzer-owned facts. It shows capacity extents, padding gaps, target-region boundaries, and
read/write/read+write selected prefixes at their actual block offsets; Unknown/separate/oversized
cases remain explicit rather than receiving fabricated geometry.

Enum declarations now support an optional durable semantic `:bit-width`; absence means auto width
derived from literal and implicit values. Individual values may be marked `:sentinel true`, including
multiple named sentinel states independently of the existing count sentinel. The shared domain
engine accounts for reserved sentinel code use, signed/unsigned ranges, fit, and unused semantic
codes independently of the target backing type.
Optional `:signed true` or `:signed false` constrains the semantic domain; absence infers signedness
from known values. Signedness participates in minimum-width derivation independently of the C++
lowering type.
Known undersized domains are rejected through normal edit rollback; general initializer expressions
remain explicitly unknown. The positional underlying type is optional and remains an explicit C++
lowering preference when present. When absent, known signedness/effective semantic width selects the
smallest legal fixed-width C++ primitive downstream; Unknown facts reject generation rather than
guessing, and no fabricated primitive dependency enters the semantic graph.

Standalone `integer-scalar` declarations in ordinary modules provide semantic domains
without a C++ underlying type. Each scalar stores signedness, an inclusive live range, auto or
explicit 1-64-bit width, and ordered named live/sentinel codes. The shared graph resolves these as
`IntegerScalarType` nodes with no target size/alignment facts; headless analysis reports live,
sentinel, required, and unused code space. Creation, flat inline editing and drag reorder,
undo/redo, preview, save, and reload use the same editable-document path. Scalars emit no C++ declarations
by default and do not fabricate a physical C++ value type. An
explicit, source-backed `:cpp-emission constants` policy can instead project the ordered named codes
as typed `inline constexpr` C++ constants. The author chooses a validated integral output type; this
projection remains downstream policy and does not add ABI size/alignment to the semantic scalar.
`constants-with-names` additionally emits an allocation-free `constexpr` value-to-symbol lookup
whose empty result honestly represents unnamed values.

`module` now provides an initial source-backed `linear-quantized` physical
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

`module` also supports source-backed `integer-varint` declarations for unsigned,
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

`module` now also supports source-backed standalone `fixed-point` declarations.
Signedness, total width, fractional width, and nearest-even/toward-zero rounding resolve to a
first-class semantic representation without a fabricated C++ primitive or ABI `sizeof`. Headless
analysis reports exact raw range, whole/fractional/sign allocation, scale, resolution, numerical
range, rounding-error bound, and overflow-safe selected-count encoded bits. Creation, flat editing,
duplication, rename, draft deletion, undo/redo, preview, save/reload, browser/graph navigation, and
end-to-end analysis are implemented. Existing fixed-point declarations are also selectable as
packed field types through an explicit `fixed-point` kind. Placement retains their graph identity,
uses the exact total width, reuses headless numerical facts, and generates signed/unsigned `*_raw`
scaled-integer APIs with boundary validation rather than pretending to return decoded values.
Plain packed integer fields can create a new shared fixed-point declaration with their signedness
and width prefilled, then bind to it through two undoable edits; invalid binding rolls back the
declaration. Existing semantic ranges/codes/relationships disable contextual conversion to avoid
silently discarding meaning.

`module` now supports source-backed `mini-float` representations. Explicit
sign, exponent, significand, and bias fields use an initial IEEE-style policy with distinct zero/
subnormal, normal, and infinity/NaN exponent roles. Resolution creates a first-class semantic node
without inventing a compiler primitive or ABI `sizeof`; analysis reports exact code roles and
exponent facts, numerical extrema where the host can represent them, rounding/resolution facts, and
overflow-safe selected-count payload bits. Typed creation/replacement/deletion, token-local source
preservation, duplication, rename, undo/redo, preview, save/reload, browser/graph navigation, and a
flat inline Properties editor are implemented.
Explicit `mini-float` packed placement now retains the first-class graph identity and exact
encoding width, rejects competing field-local integer semantics, embeds headless mini-float facts,
and generates an encoded-only unsigned integer API. Source preview/save/reload and generated
runtime preserve all code patterns; decoded floating arithmetic remains future policy.
Plain packed integer fields of sufficient width can also create and bind a shared mini-float with
a valid prefilled bit partition; binding failure rolls back creation. The shortcut is unavailable
when local domain metadata would otherwise be silently discarded.

`module` also supports source-backed `optional-sentinel` declarations. Each one
references an integer scalar and selects one of its named sentinel codes as the absence state.
Validation rejects non-scalar sources plus missing or non-sentinel codes; resolution retains the
exact code, source-derived width, and dependency. Analysis separates present values, the absence
code, other sentinels, and unused codes, and scales encoded payload bits without inventing ABI
storage. Typed history/source persistence, scalar-rename repair, creation, duplication/deletion,
flat constrained source/sentinel editing, graph navigation, and Layout/Properties presentation are
implemented.

`module` also supports source-backed `optional-presence-bit` declarations as a
distinct sibling policy over an integer scalar. Resolution derives one presence bit plus the source
payload width, including 65 encoded bits for a 64-bit source. Analysis separates one canonical
absence state from redundant absent payload patterns, source sentinel/unused code space, and
overflow-safe selected-count encoded bits without inventing ABI placement. Typed history/source
persistence, scalar-rename repair, creation, duplication/deletion, flat source editing, graph
navigation, and Layout/Properties presentation are implemented. Byte packing and bit order remain
future placement facts. Any two same-source sentinel/presence declarations can be compared through
headless analysis and the Comparison view; policy roles, encoded width, redundant absence patterns,
and selected-count payload-bit deltas remain separate from unspecified allocated storage.

The shared LispB schema supports `record` declarations in ordinary modules. Records contain semantic
members, optional fixed element counts, and optional source-backed semantic relationships. They
resolve to `RecordType` nodes and labeled dependency edges and
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
The Comparison view can independently load a second target profile and show the same record's
member size/alignment/offset/extent/padding, complete object layout, aggregate storage/padding,
cache-line/page behavior, cache-capacity fit, and checked physical deltas at the shared selected
count. Missing facts remain Unknown and semantic-member mismatches reject the whole comparison.
Packed fields, reusable standalone integer scalars, record members, and standard-library SoA columns
now support optional source-backed semantic relationships to declared types through the same shared
relation schema/graph value. Initial kinds
cover index/count/offset, discriminant, containment/membership, quantisation/encoding, and general
reference edges. All four owners round-trip, validate, participate in rename/deletion safety,
appear as labeled graph dependencies, and are editable/navigable inline. Record and SoA-column
relationships remain ABI/storage-neutral metadata. Unsigned `index_into`, `count_of`,
and explicitly unit-bearing `offset_into` fields/scalars consume explicit analyzer-derived SoA
target facts per physical variant. The headless derivation owns the resolved-graph walk and SoA
physical analysis; the UI supplies session inputs only. Element offsets use capacity; byte offsets use the selected
allocation strategy's checked total allocation extent. Analysis distinguishes live indices/offsets
from terminal-inclusive counts, adds named sentinel states, handles exact and excess `2^64`
requirements without overflow, diagnoses collisions/range/width failures, and exposes the
derivation in packed bit tooltips/Comparison and scalar/field Properties/Layout. Scalar Comparison
also applies variants A/B to the same durable domain and reports checked capacity and minimum-width
deltas plus fit transitions without inventing standalone storage. Packed and scalar views also show
the current-width code-space target-capacity limit after sentinel/terminal states and remaining
code-space headroom. Separate zero-based semantic-range and concrete-sentinel limits combine into an
effective valid-capacity/extent boundary only when all facts are known, so a packed field without a range
does not receive optimistic semantic headroom. This remains planning state: it does not write
capacity/extent into the semantic graph or silently change durable auto widths.

The shared schema also has source-backed raw `union` declarations in ordinary modules. Ordered
alternatives may be scalar or fixed arrays, resolve to first-class dependency-bearing semantic
nodes, reject direct and mixed record/union by-value cycles, and lower to ordinary C++ unions.
Stable-ID commands, preview/save/reload, creation, duplication/deletion, and the flat alternative
grid cover name/type/count editing, type navigation, and button/drag reorder. Target analysis
derives maximum alternative extent, alignment-rounded size, tail padding, and per-alternative union
slack while leaving unknown/overflowed facts explicit. Selected-count analysis scales storage,
tail padding, and conditional per-alternative slack, and reports target-driven cache-line/page
footprints and boundary crossings without inventing an alternative distribution. A separate
session-only, stable-declaration-keyed alternative workload is now available when the user supplies
weights explicitly. Headless analysis validates unique known alternatives, preserves Unknown and
overflow, and reports per-row extent/slack, checked weighted totals, and conditional per-value/
selected-count expectations. Layout and Properties show the workload, while Comparison applies the
same exact weights to both target profiles only after an all-or-nothing identity/count/weight join.
This workload is not schema state and does not imply a stored discriminant or performance result.
Session workload keys now reconcile whenever the editable graph changes: raw alternative renames
and tagged tag reassignments migrate a weight when unambiguous, deletion prunes stale keys, and
reorder/property edits preserve the current workload across apply/undo/redo synchronization.

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

Generation policy is source-backed and authorable inline. Reflection, enum-array helpers, native
API mode, export specifier, and conversion functions all pass through ordinary `ReplaceEnum`
validation/history. Conversion changes normalize into shared canonical order and preserve stable
multiline conversion rows and comments; incompatible policies never enter history.

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
codes, is implemented. Reusable standalone named-value/range-constrained integer scalars can now be
placed in packed fields with shared domain validation, analysis, generated accessors, and contextual
creation/binding. Linear-quantized representations can now be placed in packed fields through an
explicit kind. Placement retains the representation graph identity, derives its exact encoded width,
reuses the quantization analyzer, and lowers an honestly named raw encoded-code API with reserved-
code validation. Fixed-point representations likewise retain identity and exact total width while
lowering only explicit signed/unsigned scaled `*_raw` accessors and reusing fixed-point analysis.
Decoded-value helpers and non-packed container placement remain future work.

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
- **DONE:** Allow creation of a referenced enum from the field workflow, with width-prefilled shared
  enum creation, immediate packed-field binding, rollback on bind rejection, and ordinary two-step
  undo/redo.
- Save and reload the resulting LispB without losing semantics.

The primary acceptance case is creating an enum backed by `uint8`, then creating a packed `uint32`
value with a 24-bit integer field and an 8-bit field that semantically references that enum.

## 4. SoA authoring

The initial standard-library SoA vertical slice is implemented. Selected nested columns expose
optional fixed-schema and nested-schema references inline, and switching back to array clears those
nested-only properties. Selected columns also expose source-backed semantic relationship
kind/target/unit editing, type picking, clearing, and navigation. Those links add graph/reference
safety without turning capacity or allocation placement into durable schema. Coordinated generated
field-mask/storage/dimension authoring remains
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
defaults and C++ trailing-default order before a document command is accepted. Body fragments and
registered dependency keys are separate flat ordered editors with resizable direct text input,
add/duplicate/delete, and button/drag reorder; unknown or blank dependency keys are rejected by the
shared validator. Optional trailing returns atomically switch the declared return to/from `auto`,
support shared type picking, and remain valid across enable/disable; opaque template-parameter and
requires-clause text is authorable with resizable inline inputs and shared nonblank validation.
Stable uniquely named function and parameter rows preserve surrounding source comments; list-form
function properties patch locally. A conservative one-to-one unmatched-row matcher also preserves
function and parameter blocks during unambiguous direct renames; overload ambiguity, multiple
simultaneous unmatched rows, and ambiguous rename/reorder combinations retain canonical fallback.
Unchanged raw `#cpp` bodies retain their exact spelling through unambiguous function and parameter
renames. Editing a raw body replaces only its bounded property value with the canonical quoted-list
form, preserving the containing function's comments and other properties. Multiline list-form body
and dependency entries preserve stable unique source blocks and comments through structural edits
and one direct value edit; duplicates or ambiguous mappings canonicalize only the affected list.

- Create standard-library SoA declarations.
- Add, remove, duplicate, and reorder columns.
- Select semantic column types and supported column kinds.
- **DONE:** Navigate to referenced types and create a shared record directly from an ordinary SoA
  array-column workflow.
- Keep planning-only capacity experiments separate from source semantics unless capacity becomes a
  declared LispB property.
- Save, reload, analyze, and generate the declaration.

## 5. Relationship and declaration lifecycle operations

An initial dockable pan/zoom relationship graph is implemented over the resolved semantic graph,
with selectable nodes and field-aware dependency labels. Edge labels now provide hover source/
target inspection and click-through target navigation without owning semantic state; **Fit all**
frames the complete simple column layout. A flat case-insensitive type/module/namespace/kind search
cycles and focuses matching nodes while retaining the complete topology. Nodes can be left-dragged
into stable-identity manual world positions; Fit/Focus use those positions, **Automatic layout**
discards them, and the existing ImGui settings store persists escaped positions per normalized
project path while ignoring malformed or stale identities. Selecting a node colors its direct
dependencies, users, and bidirectional/cyclic neighbors distinctly and dims unrelated topology
without hiding or disabling it; every role is derived from the current graph. Packed-field relationship editing and the
initial shared relationship-kind set are implemented. Duplication, supported-kind rename,
compatible cross-namespace module movement, and reverse-user/registered-alias-safe deletion of draft
or source-backed declarations are implemented; capacity-derived consequences remain future work.

- Provide a searchable semantic type picker.
- Navigate from a field or column to its referenced definition.
- Rename declarations while updating semantic references deliberately.
- Report all reverse users before deletion.
- Reject unsafe deletion or require explicit reference repair.
- Duplicate declarations under a new stable identity.
- **DONE:** Move editable declarations between compatible existing modules, preserving stable
  identity/history and exact cross-file source through save/reload. Namespace-changing moves repair
  supported graph-derived references and reject aliases or local links outside source-aware repair.
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
follow the declaration, and validates generated-name collisions transactionally. The shared source
boundary now patches a validated owner's atomic name token and retries its ordinary source-preserving
renderer, retaining comments, custom whitespace, stable child blocks, and localized repaired-user
edits across all supported kinds. Registered-alias repair remains a lifecycle extension.
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
physical override map. Session physical variants can also be compared under one explicit selected-
column SoA workload, including independent capacity/type overrides and factual footprint/slack
deltas. General typed semantic change sets remain future work.

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

Initial explicit access sets are implemented for packed values, AoS records, and standard-library
SoAs. Packed analysis separates selected useful bits from the whole containing-word array footprint.
Record analysis computes exact touched-region unions for a contiguous aligned object array. SoA analysis
uses the distinct physical model of one allocation per selected column and reports minimum region
footprints; element count remains session workload state and is not conflated with SoA allocation
capacity or persisted into LispB.
When a standard-library SoA declares an equivalent type that resolves to a record, the Layout view
also applies the same selected names and count to both models and shows useful bytes, cache lines,
cache footprint, pages, page footprint, and signed deltas side by side. The UI explicitly labels the
AoS aligned-array results as exact and the independent SoA allocation results as lower bounds; it
does not turn those physical facts into a speed claim.
The Comparison view also applies one selected-column workload to any two session SoA physical
variants. Independent variant capacities and physical column overrides remain intact while useful
payload, minimum cache/page footprints, allocated payload, capacity slack, and checked deltas are
shown side by side.

## 9. Physical-fact accuracy

- Record provenance for every ABI fact. Primitive facts carry per-type provenance and memory facts
  carry independent group provenance.
- Load generated `sizeof` and `alignof` facts from the real target compiler and configuration. The
  initial probe, deterministic profile format, transactional loader, and explicit session selection
  are implemented; broader project/profile discovery remains follow-up work.
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
