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

Window size, panel placement, dock split ratios, and text size are saved in the per-user application
preferences directory and restored on the next launch. Maximized, minimized, and fullscreen sizes
do not replace the last normal window size. Use **View > Reset panel layout** to return to the
default panel arrangement.

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
   chooser plus direct navigation while retaining free-text references. Record members support
   scalar or fixed-array cardinality and update the byte map live. Properties can duplicate and
   rename supported declarations. Supported declarations can also be deleted after confirmation
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
   machine-specific capacities Unknown.
   Record maps derive member offsets, fixed-array extents, alignment, internal padding, and tail
   padding from the active target profile. The shared element-count control scales record storage,
   member extents, padding, minimum cache lines, and minimum pages with overflow-safe arithmetic.
   It also counts elements crossing cache-line and page boundaries for a contiguous array with an
   explicitly stated region-aligned base assumption.
   The record member grid's Access column defines a sequential access set (defaulting to the
   focused member) and reports useful selected bytes, enclosing AoS footprint, exact distinct
   cache lines/pages touched, and non-selected bytes in the touched cache lines without claiming a
   speedup.
   For packed values,
   choose an element-count preset or custom count to see overflow-safe storage, unused-bit,
   cache-line, and page totals from the explicit target profile. Comparison lets you select any two
   variants as A and B and shows factual deltas without ranking either representation. B follows the
   actively edited variant until you choose or swap it explicitly. When a linear quantization is
   selected, Comparison instead offers every declared representation of the same semantic source
   and shows code-space, precision/error, clipping-policy, and selected-count encoded-bit deltas.
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
   dependencies. Middle-drag to pan, use the wheel to zoom, and click a node to navigate to it.

The planner supports enum and standalone integer-domain inspection, packed values, records, flat
standard-library SoAs, and standard-library vector SoAs. Enum-backed packed fields retain links to
their enum definitions; physical facts are derived separately from declared underlying or storage
types. Unknown types and unsupported schemas are reported as diagnostics rather than guessed.

Enum Properties derives the known numeric domain from literal and implicit values. It separates
live symbols from named sentinels and the count sentinel, reports minimum/effective semantic width,
unused semantic and backing codes, and validates the domain against explicit signed/unsigned target
facts. Semantic
width can be auto-derived or stored explicitly as LispB `:bit-width`; it is independent of the
current C++ underlying type. Semantic signedness can likewise be auto-inferred or stored explicitly
as `:signed true` / `:signed false`; an explicitly signed `0..7` domain therefore needs four bits,
while an unsigned domain rejects known negative values. Initializer expressions that cannot be evaluated safely remain Unknown
with an explanatory diagnostic.

Column diagrams show aggregate payload only. They do not imply that standard-library vectors share
an allocation or model allocator overhead, capacity slack, or generated single-allocation gaps.
Per-column and aggregate minimum page counts use the target profile's page size; the aggregate sums
the separate standard-library vector allocations rather than pretending that they share pages.

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
2. Choose an existing enum module, enter the declared name and current lowering type, and choose an
   auto or explicit semantic width and auto/signed/unsigned semantic signedness.
3. In Properties, switch between auto/explicit semantic width and add, duplicate, delete, reorder,
   or drag values. Select a row to edit its symbolic name, explicit value, display name, serialized
   name, hidden marker, named-sentinel role, or count-sentinel role inline. Multiple values may be
   named sentinels; the count role remains unique.
4. Use **File > Undo** and **File > Redo** while designing.
5. Use **File > Preview LispB changes** to compare the original and complete updated source.
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
change width until the target has factual capacity metadata.
The baseline bit map permits divider dragging; releasing a divider commits both adjacent widths as
one undoable semantic command.

Standalone integer-scalar authoring uses **+ New integer scalar** and an existing `scalar-module`.
The declaration records a signed or unsigned inclusive live range, auto or explicit 1-64-bit
semantic width, and ordered named ordinary/sentinel codes. Its Properties grid supports inline
range, width, name, value, and role edits plus add, duplicate, delete, button reorder, and drag
reorder. Analysis reports live values, sentinel and required codes, derived width, and unused code
space. An integer scalar intentionally has no standalone `sizeof`, alignment, or mandatory C++
primitive.

Linear quantization authoring uses **+ New quantization** and an existing `representation-module`.
Choose an `integer-scalar` source, encoded width, reserved-code count, and reject/clamp clipping
policy. The declaration remains linked to the source domain in the semantic graph; it does not copy
the source range. Properties and Layout report exact usable capacity where representable, source
span, linear resolution, maximum half-step rounding error, and endpoint behavior. Edits participate
in undo/redo and source preview/save/reload. This first representation describes encoded bits but
does not invent a standalone ABI `sizeof` or generated C++ wrapper. Declare two representations of
the same source (for example Q8 and Q10) to compare them directly. The selected-count payload-bit
totals are exact and overflow-checked; allocated bytes, stride, cache lines, and pages remain
Unspecified until a container placement policy exists.

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
switching back to array removes those invalid nested-only properties. Existing mask metadata is
edited through coordinated actions: enable creates both generated type names, the required storage
column, and the first mask field in one undoable command; disable removes the whole configuration;
eligible array columns can be included or excluded while retaining at least one field. Existing mask
dimensions are edited inline with add, duplicate, delete, reorder, and direct name/extent controls.
Capacity remains a planner experiment setting and is not written into LispB.

Raw unions use **+ New union** and an existing `union-module`. Alternatives are edited inline with
the shared semantic type picker and may be scalars or positive fixed arrays. Add, duplicate,
delete, rename, change type/count, button reorder, and drag reorder all use ordinary undoable LispB
commands. Layout reports the target-derived maximum extent, object size/alignment, tail padding,
and slack for every alternative. The shared element-count presets scale storage and conditional
per-alternative slack and show cache-line/page footprints and boundary crossings for a contiguous,
aligned array. Each slack total assumes every object contains that alternative; the planner does
not invent tag frequencies. Raw unions do not yet declare a discriminant.

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
is inferred from declaration order or persisted into LispB.

Use **Duplicate declaration** in Properties to create a validated sibling copy with a fresh stable
identity and collision-free name. The operation uses the same semantic command/history path as
creation and works for the editable declaration kinds. SoAs with explicit generated helper names
remain excluded until those dependent names can be repaired deliberately.

Enums, integer scalars, quantizations, varints, packed values, and records can also be renamed
inline. Rename is an atomic semantic transaction: resolved user
references are rewritten to the new qualified spelling, every affected source declaration appears
in preview, and undo/redo restores the complete graph. Declarations bound through the separate
registered-types file are rejected until that file has the same source-aware editing path.

Creation currently targets an existing module of the matching kind; creating modules and renaming
SoAs are not yet exposed. Supported existing declarations can be deleted after reverse-user checks.
Ordinary nonstructural edits to enums, integer scalars, packed values, quantizations, varints,
fixed-point values, and both optional policies preserve declaration-local comments, whitespace, and
unchanged token spelling while patching only affected properties. Packed fields include stable
named codes, explicit reserved regions, and existing relationship forms. Enum row insertion,
duplication, deletion, and reorder preserve unchanged existing rows and their comments; new rows are
rendered canonically. Packed field/reserved insertion, duplication, deletion, and reorder likewise
preserve stable segment blocks and their comments, including unchanged named-code and relationship
text, while rendering only new segments canonically. Structural child changes in other declaration
kinds deliberately fall back to canonical declaration rendering. Record members and raw-union
alternatives preserve stable named blocks and their comments through insertion, duplication,
deletion, and reorder, rendering only new children canonically. Tagged-union alternatives now use
the same stable named-block behavior for structural edits while preserving declaration-level
discriminant/export source independently. Ordinary standard-library SoAs preserve stable named
member blocks through structural edits, with the editable region bounded before untouched custom
functions and advanced forms whose semantics are proven unchanged. Unsupported edits remain visible as canonical
fallbacks in preview. Unrelated declarations and files are left unchanged.

Closing the application with dirty semantic edits offers Save, Discard, and Cancel choices. The
Graph view can be shown or hidden from View; docking and visibility are persisted.
