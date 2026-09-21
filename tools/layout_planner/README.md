# Memory layout planner

The memory layout planner inspects and compares physical memory layouts derived from LispB schemas.
It is a standalone Windows application and does not depend on Unreal Engine, Qt, or C#.

See [architecture](ARCHITECTURE.md) for implementation and dependency details.
See the [roadmap](ROADMAP.md) for the path from inspection and experiments to full LispB type
authoring.

## Build and executable location

From the repository root, initialize the optional UI dependencies and run the dedicated workflow:

```powershell
git submodule update --init native/third_party/sdl native/third_party/imgui
cmake --workflow --preset layout-planner
```

The workflow builds and tests the planner. The executable is worktree-local:

```text
%REPOSITORY_ROOT%\out\build\layout-planner\tools\layout_planner\app\layout-planner.exe
```

`%REPOSITORY_ROOT%` is the root of the current worktree. The planner is not installed system-wide
or added to `PATH`.

## Run

Run the planner with the repository root as its working directory:

```powershell
.\out\build\layout-planner\tools\layout_planner\app\layout-planner.exe
```

The default invocation loads `lispb/project.lispb` and its `sandbox-code` target. Override either
selection when inspecting another manifest or target:

```powershell
.\out\build\layout-planner\tools\layout_planner\app\layout-planner.exe `
  --project path\to\project.lispb `
  --target target-name
```

Use `--help` to list command-line options. Project-load and schema diagnostics are reported in the
application and on standard error.

Window size, panel placement, dock split ratios, panel visibility, and text size are saved in the
per-user application preferences directory and restored on the next launch. Every major workbench
view can be shown or hidden from **View**. Maximized, minimized, and fullscreen sizes do not replace
the last normal window size. Use **View > Reset panel layout** to restore the default views and
arrangement.

## Getting started

1. Start the planner from the repository root. The Project / Schema panel lists LispB enums,
   standalone integer scalars, linear-quantized, integer-varint, fixed-point, mini-float,
   sentinel-encoded, and presence-bit optional representations, packed values, records, and
   supported standard-library SoAs from the shared semantic graph.
2. Select `EntityUniqueId` to inspect its proportional packed-bit layout, or `WorldAABBsColumns`
   to inspect its six SoA columns.
3. Enum, integer-scalar, linear-quantized/integer-varint/fixed-point/optional
   representation, packed-value, record, and standard-library
   SoA baselines are editable LispB declarations.
   Their inline Properties editors issue validated semantic commands and participate in File >
   Undo/Redo. Packed fields, record members, and SoA columns offer a searchable graph-backed type
   chooser plus direct navigation while retaining free-text references. The chooser exposes
   uniquely addressable declarations across modules with their qualified source spelling instead
   of treating them as external leaves. Record members support
   scalar or fixed-array cardinality, and both record members and SoA columns support optional
   durable semantic relationships. Relationship metadata adds labeled graph edges and reference
   safety without changing target-derived member offsets, SoA storage, or session allocation
   placement. Properties can duplicate and
   rename supported declarations. When another compatible module exists, the Module selector moves
   the declaration through the same undoable source-backed command path and repairs graph-derived
   references when the destination namespace differs.
   Supported declarations can also be deleted after confirmation
   when the resolved graph has no direct users or registered alias; deletion is a normal undoable
   semantic command for both existing source and newly created declarations.
4. Use **Create editable variant** for an in-memory physical experiment. In a variant, change a
   packed field width/storage type or a SoA capacity/column type without modifying LispB. Schema
   values, planning values, and active overrides are shown separately.
5. Layout shows linked packed-bit, record byte maps, aggregate column-payload, and cache-line views.
   Its Target profile table reports platform, architecture, ABI, compiler, and build configuration
   independently, showing Unknown for any identity fact the profile does not supply. Primitive and
   memory fact provenance are reported separately from that identity. Optional L1-data/L2/L3
   capacity facts drive factual whole-working-set fit rows; the built-in baseline leaves those
   machine-specific capacities Unknown. Build and run `layout-profile-probe` with the compiler and
   configuration you want to inspect, save its stdout as a profile, then load that file from the
   Target profile panel to use its explicit primitive and memory facts. Malformed profiles are
   rejected transactionally. The chosen profile path is remembered independently for each project;
   a missing or invalid restored file falls back to the built-in profile with a diagnostic. Expand
   **Primitive facts** to inspect every exact size, alignment, integer role, value width,
   representation alias, and provenance. The editor also accepts session-only cache-line, page,
   and cache-capacity facts in bytes, with empty fields remaining Unknown; these inputs immediately
   refresh analysis but never modify LispB or semantic dirty history.
   Record maps derive member offsets, fixed-array extents, alignment, internal padding, and tail
   padding from the active target profile. The shared element-count control scales record storage,
   member extents, padding, minimum cache lines, and minimum pages with overflow-safe arithmetic.
   It also counts elements crossing cache-line and page boundaries for a contiguous array with an
   explicitly stated region-aligned base assumption.
   The record member grid's Access column defines a sequential access set (defaulting to the
   focused member) and reports useful selected bytes, enclosing AoS footprint, exact distinct
   cache lines/pages touched, and non-selected bytes in the touched cache lines without claiming a
   speedup. With a record selected, Comparison can load a second generated target profile for the
   session and show the same semantic record's member sizes/alignments, offsets, padding, object
   layout, aggregate footprint, cache-line/page facts, cache-capacity fit, and checked B-minus-A
   deltas. Unknown target facts remain Unknown, and the table reports physical consequences rather
   than predicting performance. The same section applies the current selected-member read/write
   intent map and multiplicity to both targets. It compares useful and logical bytes, enclosing AoS
   storage, exact unique/read/write cache-line and page address coverage, non-selected footprint,
   and cache fit without treating address coverage as measured traffic.
   Raw unions use the same profile A/B workflow and selected-count scale. Comparison reports each
   alternative's target-derived element size/alignment, extent, conditional slack, object size and
   alignment, aggregate storage/tail padding, cache/page boundary behavior, and cache fit. It does
   not label the profiles ABI-compatible or predict performance. Optional session-only alternative
   weights are applied unchanged to both targets and compare checked weighted extent/slack totals,
   distribution-derived per-value expectations, and selected-count expectations without implying
   that the raw union stores a discriminant.
   Tagged unions additionally hold discriminant identity and tag coverage fixed while comparing
   discriminant storage, payload-union layout, object padding, alternative payload slack, and
   aggregate physical facts. Mapped, unmapped, sentinel, and count-sentinel roles remain semantic
   facts. When an explicit session distribution exists, the same tag weights are applied to both
   target layouts and compared by per-entry, weighted-total, expected-per-value, and selected-count
   extent/slack facts.
   Enums also use the target A/B picker while holding their complete semantic domain, ordered
   values, sentinel roles, signedness, semantic width, and explicit-or-derived C++ backing spelling
   fixed. Comparison keeps semantic code-space facts visibly separate from target backing size,
   alignment, storage bits, unsigned value-bit capacity, backing fit, and unused backing codes.
   A semantic width therefore never acquires a fabricated standalone `sizeof`, and Unknown target
   facts remain Unknown. The standard element-count control additionally models a contiguous array
   of the generated standalone C++ backing, with checked bytes, cache/page footprint, aligned-origin
   straddling, and cache fit. This aggregate does not imply bit-packed enum storage.
   For packed values,
   choose an element-count preset or custom count to see overflow-safe storage, unused-bit,
   cache-line, and page totals from the explicit target profile. The same explicitly region-aligned
   contiguous-array model reports how many packed elements cross cache-line and page boundaries;
   missing target facts remain Unknown. The packed field grid also defines a session access set with
   per-field Read, Write, or Read + write intent. Layout reports selected useful bits separately
   from the complete packed-word array and its minimum cache-line/page address coverage; it never
   treats a bitfield as independently fetched storage. Matching operation-color bands on the
   baseline and variant bit maps make that workload spatially visible while remaining explicitly
   labeled as intent rather than fetch regions. Comparison applies the same packed-field
   workload to both selected variants and shows checked useful-bit, containing-storage,
   cache/page-coverage, and cache-fit facts side by side; width and storage changes remain distinct,
   and Unknown inputs never become fabricated deltas. The same view separately holds the active
   physical variant fixed and applies target profiles A/B to it, exposing storage fact, segment
   position, overflow/unused-bit, aggregate, cache-line, page, and relationship-capacity changes
   without conflating target choice with a schema/variant edit. The exact same selected-field
   workload is also applied across those profiles, reporting useful/logical bits, containing-word
   storage, waste categories, minimum read/write cache/page coverage, and cache fit without
   implying sub-word fetches. Non-useful bits are split into unselected
   ordinary fields, explicit reserved regions, and physically unused backing bits rather than
   conflating three different facts. Comparison lets you select any two variants as A and B and
   shows factual deltas without ranking either representation. B follows the
   actively edited variant until you choose or swap it explicitly. When a linear quantization is
   selected, Comparison instead offers every declared representation of the same semantic source
   and shows code-space, precision/error, clipping-policy, and selected-count encoded-bit deltas.
   Standard-library SoAs use the same target A/B picker while holding the active variant, capacity,
   allocation strategy, selected columns, access classification, element count, and multiplicity
   fixed. Comparison reports per-column size/alignment/offset/padding, aggregate allocation and
   cache/page facts, and selected-workload useful, logical, coverage, straddling, and capacity-slack
   consequences. This target section remains separate from the existing same-target physical-
   variant comparison and makes no performance claim.
   Fixed-point declarations report signedness, whole/fractional bit allocation, exact raw range,
   scale, resolution, representable numeric range, rounding policy/error, and overflow-safe
   selected-count payload bits without fabricating an ABI `sizeof`.
   Mini-float declarations report explicit sign/exponent/significand allocation, exponent-code
   roles, bias and normal exponent range, finite extrema, subnormal/normal thresholds, resolution,
   relative rounding error, and selected-count payload bits. They deliberately do not imply native
   compiler arithmetic, byte order, alignment, or an ABI `sizeof`. Create them in an existing
   representation module and edit all four encoding properties inline with ordinary undo/redo,
   source preview, and validated save/reload.
   Sentinel-encoded optionals select one named sentinel from an integer scalar as the absence state.
   They report present values, other reserved sentinel codes, unused code space, derived width, and
   selected-count payload bits without treating code-space roles as allocated byte waste.
6. Graph shows the resolved semantic types and their labeled underlying/storage/field/column
   dependencies. Left-drag nodes for a project-scoped persisted manual arrangement, middle-drag to pan, use
   the wheel to zoom, use **Fit all** for an overview, and click a node to navigate to it.
   **Automatic layout** discards manual positions. Search by type, module, namespace, or kind, then press Enter or
   use **Next match** to cycle through and focus matching nodes without hiding the rest of the graph.
   For the selected node, outgoing dependencies are green, incoming users are orange, and nodes in
   both directions are purple; unrelated topology is dimmed but remains fully interactive.
   Hover an edge label to inspect its source, target, and derived relationship text; click the label
   to navigate to its target.

The planner supports enum and standalone integer-domain inspection, packed values, records, flat
standard-library SoAs, and standard-library vector SoAs. Enum-backed packed fields retain links to
their enum definitions; physical facts are derived separately from optional explicit C++ backing or
other storage types. Unknown types and unsupported schemas are reported as diagnostics rather than
guessed.

Enum Properties derives the known numeric domain from literal and implicit values. It separates
live symbols from named sentinels and the count sentinel, reports minimum/effective semantic width,
unused semantic and backing codes, and validates the domain against explicit signed/unsigned target
facts. An enum may omit its old positional C++ backing type entirely; generation then selects the
smallest legal 8/16/32/64-bit signed or unsigned primitive from the known semantic width and
signedness. If either is Unknown, validation rejects derived lowering rather than guessing. Semantic
width can be auto-derived or stored explicitly as LispB `:bit-width`; it is independent of the
optional C++ backing type. Semantic signedness can likewise be auto-inferred or stored explicitly
as `:signed true` / `:signed false`; an explicitly signed `0..7` domain therefore needs four bits,
while an unsigned domain rejects known negative values. Initializer expressions that cannot be evaluated safely remain Unknown
with an explanatory diagnostic.
Layout can scale the current target's generated standalone backing across the shared count presets.
Those physical bytes and cache/page facts remain separate from semantic width and from enum fields
embedded inside another packed representation.

Under the default separate-column strategy, column diagrams show aggregate payload only and do not
imply that standard-library vectors share an allocation. Neither strategy models allocator headers,
growth beyond the selected capacity, heap overhead, or generated backend-specific gaps.
Per-column and aggregate minimum page counts use the target profile's page size; the aggregate sums
the separate standard-library vector allocations rather than pretending that they share pages.
An explicit session access set reports useful/unselected payload, capacity slack, cache-line/page
footprints, and per-column boundary crossings at the current element count. Each accessed
packed field/member/column can be classified inline as Read, Write, or Read + write; the shared
operation control supplies a default and can update the full selected set. Mixed classifications
report useful payload in each requested category without changing unique storage totals or assuming
a cache write policy. A positive accesses-per-element multiplicity scales separate overflow-checked
logical useful-byte totals while physical cache/page address coverage stays unchanged.
Read and write cache-line/page coverage is also reported separately: exact for the aligned
contiguous AoS model and as a separate-allocation minimum for SoA. These are address-set facts, not
write-allocation, eviction, bus-traffic, or performance estimates.
Boundary-crossing counts state the additional assumption that each separate column allocation
begins on the corresponding cache-line/page boundary; neither these counts nor footprint deltas are
performance predictions.
The session Allocation strategy control can retain that separate-column lower-bound model or model
one aligned contiguous column block. The contiguous strategy uses target type alignment and the
modeled capacity to show column offsets, inter-column padding, total block bytes, one allocation,
and exact selected-prefix cache/page unions from a region-aligned block origin. It does not mutate
LispB. Allocator headers, growth slack, heap overhead, and performance remain unspecified.
For the contiguous strategy, a scrollable Cache-line / page block map shows target-region
boundaries, complete capacity-sized column extents, alignment gaps, and the selected read/write
prefixes. Region zoom can expose small fields without turning the view into a traffic or speed
estimate; separate allocations intentionally have no single address map because their origins are
unknown.

Layout variants exist only in memory and closing the application discards them. Semantic authoring
changes remain in an editable draft until the user explicitly previews and saves them from the File
menu. See [architecture](ARCHITECTURE.md) for supported constructs, technical limits, and
implementation details.

The File menu can open a LispB project manifest by path, switch among the 20 most recently opened
projects, save the current draft, or save it as an independent clone. The most recent successful
project is reopened on the next normal launch; an explicit `--project` command-line argument takes
precedence. Until a native file picker is added, Open and Save As use path text boxes. Save As writes
a new project manifest plus a sibling `<name>_schema` directory containing the current schema
target's types and module sources, validates the clone, and then switches the planner to it. Existing
destinations are not overwritten.

The Project / Schema browser is organized by LispB module. Each module contains its declared enums,
integer scalars, physical representations, packed values, records, or supported SoA types, matching
source ownership and the destination used when new types are authored.

## Semantic authoring

Enum authoring is available as an inline table:

1. Select **+ New enum** in Project / Schema.
2. Choose an existing enum module, enter the declared name, choose derived or explicit C++ backing,
   and choose auto/explicit semantic width plus auto/signed/unsigned semantic signedness.
3. In Properties, switch C++ backing and semantic width independently; edit reflection,
   enum-array, native-API, export, and generated-conversion policy inline; then add, duplicate,
   delete, reorder, or drag values.
   Select a row to edit its symbolic name, explicit value, display name, serialized name, hidden
   marker, named-sentinel role, or count-sentinel role inline. Multiple values may be named
   sentinels; the count role remains unique. Policy changes use shared schema validation and
   ordinary undo/redo rather than planner-owned generation state, so incompatible native/Unreal
   combinations are rejected transactionally with a semantic diagnostic.
   Native plain enums can also stage, add, edit, or remove their complete Unreal projection
   subform inline: projected name, generated header path/include, conversion-header path,
   native-header include, and UEnum/Blueprint reflection mode. These are generated consumer-output
   paths and do not alter semantic width or target layout.
4. Use **File > Undo** and **File > Redo** while designing.
5. Use **File > Preview LispB changes** to reveal the dockable Source view and compare the original
   and complete updated source while continuing to author in other panels.
6. Use **File > Save LispB changes** to validate the rendered sources, replace the affected source
   file, and reload the semantic document.

Packed-value authoring follows the same path. Use **+ New packed value**, choose an existing packed
module and storage type, then edit storage/invalid value and add, duplicate, delete, reorder, or drag
fields inline. Field name, semantic type, bit width, kind, and range-helper metadata are editable.
Signed integer fields use LispB `:kind signed` with an explicit or derived 1-64-bit width, allowing physical
representations such as a 17-bit two's-complement `std::int32_t` field. Analysis shows the exact
signed range, and generated getters/setters sign-extend and reject out-of-range values without
requiring a C++ `int17` primitive.
Use **+ Reserved** for named future-use regions; they participate in physical offsets but have no
semantic type or generated getter/setter. The bit map and scale analysis distinguish semantic
payload, explicit reserved bits, and implicit trailing unused bits.
Packed declarations can explicitly choose LSB-first or MSB-first segment allocation. MSB-first
places the first declared segment at the high end of storage; generated numeric getters/setters,
reported ranges, unused-bit placement, and divider dragging follow that choice. Optional little- or
big-endian byte order describes serialized bytes only: it is displayed and persisted but does not
change the host integer's ABI or fabricate a serializer. Both properties are editable during
creation or inline and participate in ordinary undo/redo and source-preserving save/reload.
A signed or unsigned field can also define an inclusive semantic range in the inline table (for example
`-100..100`). LispB stores this as `:minimum` / `:maximum`; generated setters reject out-of-range
values. The Named codes grid attaches symbolic constants to ordinary integer values without
turning the field into a C++ enum. Sentinel codes must sit outside the live range; they remain valid
setter/raw values and participate in required-width and unused-code calculations. For example,
`(code Invalid :value 255 :sentinel true)` reserves a named invalid state for an otherwise `0..100`
field. Named-code rows support inline editing, duplication, deletion, button reorder, and drag
reorder through normal undoable schema commands. The bit-map tooltip reports live, sentinel,
required, and unused code counts separately from physical bit waste.
The field's **Auto** width option writes `:bits auto`. For signed and unsigned integers it derives the concrete
packed width from the live range and extreme named/sentinel codes; for enum fields it consumes the
enum's semantic width. The editor and bit-map show the derived width, and validation rejects auto
when the semantic domain is not known.
The selected field can also carry a durable semantic relationship such as
`(relation index_into EntityTable)`. Relationship kinds include index/count/offset links,
discriminants, containment/membership, quantisation/encoding, and general references. Targets must
resolve to declared semantic types. The inline editor supports kind/target editing, type picking,
navigation, and clearing; Graph labels the resulting dependency edge. A relationship does not
rewrite the durable width. When `index_into` or `count_of` targets a SoA, the planner applies that
variant's session capacity and reports live values, sentinel-inclusive required codes, minimum
capacity-derived width, and whether the planned width fits. `index_into` uses `0..capacity-1`, while
`count_of` includes the terminal count. `offset_into` requires an explicit durable `:unit elements`
or `:unit bytes`; element offsets use the variant's SoA capacity, while byte offsets use the checked
total allocation extent for the selected separate-column or aligned-contiguous allocation strategy.
Incomplete physical facts leave byte extent Unknown. The bit-map tooltip, selected-field Properties, and variant
Comparison expose threshold changes, the current-width code-space capacity limit, and remaining
code-space headroom. Separate semantic-range and concrete-sentinel limits explain tighter
constraints; an effective valid-capacity limit/headroom appears only when every constraint is
known. Unsupported or missing capacity facts remain Unknown.
The baseline bit map permits divider dragging; releasing a divider commits both adjacent widths as
one undoable semantic command.
Packed values, records, raw unions, and tagged unions expose their optional generated export
specifier directly in Properties. Empty text clears it; malformed identifiers are rejected by the
shared schema validator without changing the draft or history.
Packed values also expose the existing mutable-API policy inline. Enabling it generates fallible
construction/mutation and checked setters; it does not alter the semantic domain, packed bit map,
object storage, or target analysis.
For an ordinary selected field, **Create enum for selected field...** opens the existing declaration
creation workflow with the resolved field width prefilled. **Create and use** adds the shared enum,
then changes the field's type/kind metadata through the normal packed replacement command; the two
durable semantic changes remain separate Undo/Redo steps. If field binding is rejected, the
just-created enum is rolled back rather than leaving an orphan declaration. Choosing an existing
declared enum in the type chooser likewise updates the field type and enum kind together.
Signed and unsigned integer fields offer the parallel **Create integer scalar for selected
field...** workflow. It prefills the field's live range, signedness, width policy, and named codes;
**Create and use** moves those domain facts into a reusable shared scalar while preserving the
field's physical placement width and relationship. Creation and binding remain separate Undo/Redo
steps, with the new scalar rolled back if binding fails.

Standalone integer-scalar authoring uses **+ New integer scalar** and an existing `scalar-module`.
The declaration records a signed or unsigned inclusive live range, auto or explicit 1-64-bit
semantic width, and ordered named ordinary/sentinel codes. Its Properties grid supports inline
range, width, name, value, and role edits plus add, duplicate, delete, button reorder, and drag
reorder. Analysis reports live values, sentinel and required codes, derived width, and unused code
space. A scalar may also carry the same durable semantic relationship as a packed field, edited
inline with type picking/navigation and shown as a labeled graph edge. Linked `index_into` and
`count_of` scalars consume the active variant's SoA capacity for an inspectable width-sufficiency
check; `offset_into` scalars use their explicit element/byte unit and the corresponding target fact.
None rewrites the source range or width. Comparison applies variants A and B to the
same scalar and shows target extent, sentinel-inclusive required codes, minimum-width deltas, and fit
transitions such as 12 to 13 bits, along with code-space capacity limit/headroom. An integer scalar
also reports the semantic-range, sentinel-placement, and resulting effective capacity boundary.
It intentionally has no standalone `sizeof`, alignment, or mandatory C++ primitive. **Emit named
constants** is an optional downstream C++ policy: when enabled with a supported integral output
type, code generation writes typed `inline constexpr` constants for the scalar's named codes without
turning the scalar into an enum, alias, or ABI-backed semantic type. The default emits no scalar C++
declarations. **Emit value-to-name lookup** additionally generates a `constexpr` lookup that returns
the symbolic code name, or an empty `std::string_view` for an unnamed value.

A packed signed or unsigned field can reference one of these integer scalars directly. The scalar
then remains the sole owner of signedness, live range, semantic width, and named/sentinel codes;
the packed field owns only its placement width and optional field behavior. Auto width follows the
shared scalar domain, analysis reports that domain on the field, and generated packed accessors use
the smallest conventional fixed-width integer that can hold the placement without assigning a
standalone ABI size to the scalar. Choosing a scalar in the field type picker updates kind and
clears competing field-local domain metadata atomically.

Linear quantization authoring uses **+ New quantization** and an existing `representation-module`.
Choose an `integer-scalar` source, encoded width, reserved-code count, and reject/clamp clipping
policy. The declaration remains linked to the source domain in the semantic graph; it does not copy
the source range. Properties and Layout report exact usable capacity where representable, source
span, linear resolution, maximum half-step rounding error, and endpoint behavior. Edits participate
in undo/redo and source preview/save/reload. This first representation describes encoded bits but
does not invent a standalone ABI `sizeof` or generated C++ wrapper. To place it in a packed value,
choose the representation in the field type picker; the field switches atomically to **linear
quantized**, uses the representation's exact width, and clears competing local domain metadata.
The packed inspector shows the shared source range, usable/reserved codes, resolution, error, and
clipping policy. Generated packed APIs deliberately expose only `*_encoded` code accessors and
reject the high-end reserved-code interval—they do not silently treat the code as a decoded source
value. Declare two representations of
the same source (for example Q8 and Q10) to compare them directly. The selected-count payload-bit
totals are exact and overflow-checked; allocated bytes, stride, cache lines, and pages remain
Unspecified until a packed or other container placement policy exists.

Variable-length integer authoring uses **+ New varint** in the same representation module. Unsigned
varint requires an unsigned integer-scalar source; signed varint and ZigZag+unsigned-varint require
a signed source. Properties and Layout report the exact minimum/maximum encoded bytes across the
live range and named sentinel codes, plus overflow-safe selected-count bounds. They intentionally
report a range, not a fixed `sizeof`. Add semantic values and occurrence weights to the session
distribution table to calculate an explicit sample total, expected bytes/value, and a selected-count
estimate. The breakdown shows encoded bytes/value and checked weighted bytes for every valid row;
workloads are planning state and are not written to LispB. Source and encoding edits,
undo/redo, preview, save, and reload use the shared editable document. If two varint declarations
share a source, Comparison shows both exact size bounds and their selected-count lower/upper deltas.
The same source-keyed session distribution is evaluated under both encodings to show factual sample,
expected-value, and selected-count estimate deltas.

Fixed-point authoring uses **+ New fixed point** with explicit signedness, total width, fractional
width, and rounding policy. Standalone analysis reports raw range, numerical endpoints, scale,
resolution, and maximum rounding error without inventing an ABI wrapper. A plain packed integer
field also offers **Create fixed point for selected field...**, prefilled with its signedness and
exact width; creation and binding are separate undoable edits, and failed binding rolls back the
new declaration. Fields with local ranges, codes, helpers, or relationships must first resolve
those semantics explicitly rather than silently losing them. To place an existing fixed-point
representation in a packed value, choose it in the field type picker. The field switches atomically
to **fixed point**, follows the exact representation width, and clears competing integer-field
range, code, helper, and relationship metadata. The packed table and detail view reuse the same
headless fixed-point facts. Generated packed APIs expose only `*_raw` scaled integer accessors with
signed or unsigned boundary validation; they do not silently return decoded real values or apply
rounding. Conversion helpers and non-packed placement remain later representation policy.

Sentinel-encoded optional authoring uses **+ New optional**. The creation flow only offers integer
scalars that already own at least one named sentinel, and the flat Properties editor constrains both
source and absence-code choices to valid shared semantic declarations. LispB stores the policy as
`(optional-sentinel Name :source Scalar :sentinel Invalid)`. The source scalar continues to own the
live range, all code values, and any other sentinel meanings; the optional declaration does not copy
or mutate them. Graph labels the dependency with the chosen sentinel, and all edits use normal
undo/redo, preview, validated save, and reload.

Presence-bit optional authoring uses **+ New presence optional** and stores
`(optional-presence-bit Name :source Scalar)`. It is a separate physical policy over the same
semantic scalar rather than a mode of the sentinel encoding. Properties derives one presence bit
plus the scalar payload width, so a 64-bit source is honestly reported as 65 encoded bits. Analysis
separates present values, one canonical absence state, source sentinel/unused payload codes, and
redundant absent payload patterns. Concrete byte packing, presence-bit ordering, ABI size, and
cache/page consequences remain unspecified until a placement policy exists. Creation, flat source
editing, duplication, rename, deletion, graph navigation, undo/redo, preview, save, and reload all
use the shared editable document. Comparison can select any same-source sentinel or presence-bit
sibling and reports the policy/code-space roles, per-value width, and selected-count payload-bit
delta while leaving placement-dependent bytes and traffic unspecified.

Standard-library SoAs can likewise be created with **+ New SoA**. Columns support inline name,
semantic type, and array/nested kind editing plus add, duplicate, delete, button reorder, and drag
reorder. Selected nested columns also expose optional fixed-schema and nested-schema references;
switching back to array removes those invalid nested-only properties. Every selected column can
also add/edit/clear a shared semantic relationship, choose an explicit offset unit, pick and
navigate to a declared target, and round-trip it through ordinary undo/redo/preview/save. Existing
mask metadata is
edited through coordinated actions: enable creates both generated type names, the required storage
column, and the first mask field in one undoable command; disable removes the whole configuration;
eligible array columns can be included or excluded while retaining at least one field. Existing mask
dimensions are edited inline with add, duplicate, delete, reorder, and direct name/extent controls.
Capacity remains a planner experiment setting and is not written into LispB.
An ordinary selected array column can also use **Create record for selected column...**. The
existing record-creation workflow starts with a collision-free suggested name and a first member of
the column's current semantic type. **Create and use** creates that shared record, then binds the
column through a separate ordinary `ReplaceSoa` history step without changing column kind or SoA
generation metadata. A rejected binding rolls the record creation back instead of leaving an
orphan declaration.

Raw unions use **+ New union** and an existing `union-module`. Alternatives and the optional export
specifier are edited inline with
the shared semantic type picker and may be scalars or positive fixed arrays. Add, duplicate,
delete, rename, change type/count, button reorder, and drag reorder all use ordinary undoable LispB
commands. Layout reports the target-derived maximum extent, object size/alignment, tail padding,
and slack for every alternative. The shared element-count presets scale storage and conditional
per-alternative slack and show cache-line/page footprints and boundary crossings for a contiguous,
aligned array. Each slack total assumes every object contains that alternative; the planner does
not invent tag frequencies. Properties can instead accept explicit session-only weights for named
alternatives. Layout then shows checked weighted extent/slack totals and expectations per value and
at the selected count; Comparison holds those exact weights fixed across profiles. Invalid names,
duplicates, incomplete target facts, zero total weight, and overflow remain explicit diagnostics.
The weights are not persisted to LispB and do not assert that a raw union stores or can recover a
runtime alternative tag. A weight follows an inline alternative rename, stale keys are pruned after
deletion, and ordinary reorder/type/count edits leave the workload intact, including through the
same document graph synchronization used by undo/redo. Raw unions do not declare a discriminant.

Tagged unions are distinct `tagged-union` declarations in the same module kind. Each names an enum
discriminant and maps every payload alternative to one non-sentinel enumerator. The graph shows the
`discriminates` edge and labeled payload dependencies. Layout derives the discriminant storage,
alignment gap, shared payload-union size/alignment, tail padding, total object facts, and conditional
payload slack from the target profile, and visualizes the tag/padding/payload byte regions. The
initial source and target model lowers to an explicit tag member followed by a nested payload union;
**+ New tagged union** chooses an existing union module, enum discriminant, initial non-sentinel tag,
and payload type. Properties then edits the discriminant and alternatives inline. Add, duplicate,
delete, rename, change tag/type/count, navigate, button reorder, and drag reorder all use the shared
undoable editable-document path; tag choices exclude sentinels and tags already assigned elsewhere
in the declaration. Preview, save, reload, rename repair, duplication, and exact source-backed
deletion preserve the same semantic mapping.
The shared element-count presets also scale total object, discriminant, payload-union, internal-
padding, tail-padding, and conditional per-tag slack bytes with checked arithmetic. Cache-line/page
footprints, aligned-array boundary crossings, and cache-capacity fit use only explicit target facts;
no tag-frequency distribution or speed claim is inferred.
Discriminant coverage is reported symbolically: mapped live tags, declared live tags with no payload
alternative, named sentinel tags, and the enum's count sentinel are separate roles. These semantic
code-space facts are not counted as allocated padding; undeclared numeric codes remain part of the
enum domain analysis rather than being invented as symbolic union states.
Properties also provides an explicit session-only weight for every mapped tag. Positive weights
produce checked sample payload-extent/slack totals, a per-tag weighted breakdown, and numerical
expected payload/slack per value and at the selected element count. Unknown, unmapped, sentinel,
count-sentinel, and duplicate tags are rejected by the headless workload analyzer; no distribution
is inferred from declaration order or persisted into LispB. Session weight identity follows an
inline tag reassignment, is unaffected by alternative rename/reorder/type/count edits, and prunes a
tag when its alternative is deleted.

Use **Duplicate declaration** in Properties to create a validated sibling copy with a fresh stable
identity and collision-free name. The operation uses the same semantic command/history path as
creation and works for the editable declaration kinds. SoAs with explicit generated helper names
remain excluded until those dependent names can be repaired deliberately.

The Properties **Module** selector moves an editable declaration to another existing module of the
same kind. The stable declaration ID survives the move; a namespace change atomically rewrites
supported graph-derived semantic references to the new qualified spelling. Preview shows the exact
owned declaration removed from its old source and inserted into the new one, with affected users in
their own source files, and undo/redo or validated save/reload cover the complete transaction.
Moves remain rejected when a registered-types alias or module-local SoA nested/fixed reference
cannot be represented safely at the destination.

Enums, integer scalars, quantizations, varints, packed values, and records can also be renamed
inline. Rename is an atomic semantic transaction: resolved user
references are rewritten to the new qualified spelling, every affected source declaration appears
in preview, and undo/redo restores the complete graph. For source-backed declarations, the owned
top-level name token is patched before the existing kind-specific preservation renderer runs, so
comments, custom whitespace, and stable child blocks survive the rename alongside repaired users.
Declarations bound through the separate registered-types file are rejected until that file has the
same source-aware editing path.

**+ New module** creates an empty enum, packed-value, integer-scalar, representation, record, union,
or standard-library SoA destination inside an existing loaded LispB source. It participates in
undo/redo, preview, atomic save, and reload; empty editable modules are valid generated-header
destinations. Ordinary **+ New ...** controls can then target it. The shared project layer can also
register or unregister an existing valid schema source, or stage a brand-new relative source, with undo/redo,
target-wide validation, exact manifest-plus-file preview, coordinated publication, failure cleanup,
and save/reload. After an empty new source is published, its first module uses the ordinary shared
module command. The Project view lists registered sources with inline unregistration and exposes
existing-source registration plus empty-source creation through a nonmodal relative-path field.
Unregistering never deletes the source file. While its source-list draft is active, global Undo/Redo,
persistent Source preview, Save, project switching, and
dirty-close protection operate coherently, and semantic editing waits until publication or explicit
discard; widgets do not pre-create files or edit the manifest directly. Supported existing
declarations can be deleted after reverse-user checks.
Ordinary nonstructural edits to enums, integer scalars, packed values, quantizations, varints,
fixed-point values, and both optional policies preserve declaration-local comments, whitespace, and
unchanged token spelling while patching only affected properties. Packed fields include stable
named codes, explicit reserved regions, and relationship add/edit/clear forms. Standalone scalar
relationships use the same local insertion/removal behavior. Enum row insertion,
duplication, deletion, and reorder preserve unchanged existing rows and their comments; new rows are
rendered canonically. A single direct same-position enumerator rename also retains the original row
block, comments, and spacing while patching its owned name token; ambiguous multi-row structural
changes keep the conservative canonical-new-row behavior. Standalone integer-scalar code rows use
the same behavior, including one unambiguous direct rename, while scalar properties and the optional
relationship remain independently source-preserved. Packed
field/reserved insertion, duplication, deletion, and reorder likewise preserve stable segment
blocks and their comments, including unchanged named-code and relationship text, while rendering
only new segments canonically. One unambiguous same-position field or reserved-region rename also
retains the complete segment block and patches only its name when every non-name semantic matches;
mixed-kind, rename-plus-edit, and multi-row ambiguity remain conservative. Within a stable field,
nested named-code insertion, duplication,
deletion, and reorder preserve stable code blocks; one unambiguous direct code rename preserves its
original block and patches the name token, while genuinely new or ambiguous rows render canonically.
The same localized path handles insertion of the first code into an empty scalar or field and
deletion back to an empty region, including placement before an existing relationship.
Unsupported structural child changes deliberately fall back to canonical declaration rendering.
Record members and raw-union
alternatives preserve stable named blocks and their comments through insertion, duplication,
deletion, and reorder, rendering only new children canonically. One unambiguous same-position member
or alternative rename retains its owned block and patches its name token. Tagged-union alternatives
use the same behavior while preserving declaration-level
discriminant/export source independently. Ordinary standard-library SoAs preserve stable named
member blocks through structural edits, with the editable region bounded before untouched custom
functions and advanced forms whose semantics are proven unchanged. One same-position direct member
rename retains its owned block when all other member semantics match. Unsupported edits remain visible as canonical
fallbacks in preview. Existing single-allocation forms also patch the owner token locally and retain
stable allocator-variant row blocks, comments, and spacing through allocator edits, insertion,
deletion, reorder, and one unambiguous same-position name-only rename. Enabling or disabling the
whole form remains a canonical localized operation. Existing fixed-layout forms likewise patch the
storage name locally; multiline container lists retain stable commented blocks through structural
edits and one unambiguous direct rename, while compact lists remain compact for unchanged or direct-
rename edits. Existing mask-dimension properties apply the same bounded-row behavior to named
`(index extent)` entries, including token-local extent changes and one extent-preserving direct
rename. Multiline `using` declaration lists also retain unique opaque-string entry blocks and
comments through structural edits and one direct value edit; the fragments remain source strings,
not invented semantic dependencies. Multiline custom-function body fragments and registered
dependency keys use the same stable quoted-row behavior, including direct edits, insertion,
deletion, and reorder. Unchanged raw `#cpp` bodies remain exact; editing one replaces only its body
property value with the quoted-list form. Multiline storage-operation lists likewise retain stable
atom rows and comments as capabilities are enabled, disabled, reordered, or directly substituted.
An unchanged `(all)` operation shorthand remains exact and expands only when its capability set is
edited. Multiline enum `:conversions` lists use the same atom-row boundary, preserving unique
conversion blocks and comments through policy insertion, deletion, reorder, and direct
substitution while compact or ambiguous lists remain localized to the property value.
Existing enum `unreal-projection` forms patch their name and five properties locally, retaining
projection/declaration comments and value rows. Adding or removing the complete form is likewise
bounded to the nested declaration rather than canonicalizing the enum.
Unrelated declarations and files are left unchanged.

Closing the application with dirty semantic edits offers Save, Discard, and Cancel choices. All
major views can be shown or hidden from View; docking and visibility are persisted. Source keeps
each affected file's Updated/Original text available in tabs, while Diagnostics consolidates
project-load, document-operation, and active-analysis messages without owning semantic state.
