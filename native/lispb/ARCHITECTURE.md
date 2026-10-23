# Lispb architecture

## Ownership and compilation

`lispb::Compiler` owns the shared source, string, syntax, declaration and C++ type stores,
the current semantic graph, backend IR, generated C++ IR and output artifacts. A compilation
selects a frontend and runs parsing, semantic checks, lowering and rendering through those
stores. Validation stops before lowering/rendering. Schema, Slate and kernel lower into
`CppIR`, then pass the completed IR to `CppRenderer` before output formatting and publication.
The IR owns construction, traversal and dependency queries; the renderer owns C++ source
emission. Material output keeps its binary format and runtime-facing data library.

Strings use one interned `StringId` for equal text, including source slices. Promoting temporary
text to retained storage preserves its ID. Views must be reacquired after growing the same
character buffer. Tokens keep exact source offsets separately from their text IDs, and macro
provenance is stored separately. Syntax forms retain token IDs and flat child ranges, not owning trees.
Slate parses expanded forms directly without a token flatten/reparse step.

Source loading, lexing and parsing are separate stages: `Sources::load` reads retained documents,
`lex` creates tokens, and `SyntaxParser` builds forms. `read_forms` coordinates these stages and
caches the resulting form range. Schema and project parsers interpret forms through the shared
`sexpr::Parser` value readers; file loaders coordinate includes without owning grammar helpers.
All frontend parsers report source spans and message handles into compiler-owned `Diagnostics`.
Schema and project parsers recover between independent declarations, collect errors, and stop
before semantic processing. `SourceError` identifies a recorded diagnostic and carries formatted
text for exception consumers; material diagnostics are exported at its standalone API boundary.

Declarations, semantic definitions, C++ nodes and expressions use typed tagged handles with
kind-specific columns. Ordered lists use flat ranges of handles. Callers access fields through
handles and columns; edits clone a payload row and publish its new handle. C++ functions
retain one signature and body shared by declaration and definition nodes. C++ type handles
carry centralized dependency and passing policies; semantic type IDs remain a separate
domain. Resolved occurrence IDs address semantic targets, physical type-use metadata and C++
emission metadata stored once in the graph, without synthetic type names or rewritten manifests.
Relationships reference these occurrences. Enum-domain codes and widths are computed once per
semantic graph from authored enumerator IDs; validation, lowering and layout analysis share them.

C++ template declarations and parameters use separate tagged handle domains. Type/value parameters,
defaults, packs, specialization names and constraints retain their structure through rendering.
Generated templates are constructed directly; authored `#cpp` template parameters and constraints
remain raw language-boundary inputs. Export specifiers are interned identifiers. Compiler function,
member and output-file flags are generated packed values checked into the source tree.

C++ type references retain qualified-name components and composed type handles alongside their
canonical spellings. `CppRenderer` owns a per-render environment store with parallel parent,
scope-kind and symbol-table columns. A symbol identifies its declaration through a tagged
handle and optionally names the environment introduced by that declaration. Lookup searches
parents for the first name, then follows introduced environments for qualified components.
The renderer shortens a name only when lookup resolves to the same declaration, and an RAII
guard restores the current environment after each nested emission region. Renderer instances
reset lookup state for each entry point and retain their indentation setting. `RenderContext`
only carries indentation depth and include groups; class membership follows the environment.
Signatures, templates and typed expressions share the renderer. Canonical spellings and dependency
policies remain independent of their displayed names. Authored C++ stays opaque; unknown raw
declarations or inherited scopes retain qualification. Generated raw declarations can describe
their bindings explicitly through `CppIR::set_scope_bindings`. This metadata must list all
introduced bindings. Missing names in a scope with unknown bindings produce an unknown lookup
result, preventing an unsafe search through its parent environments.

`NodeId` identifies a generated node. `Modules` and `CppFiles` retain output assembly metadata in
the same C++ IR owner, with file/module handles, shared text and flat ordered node/text ranges.
Final standalone artifacts acquire their own path/content only at publication.

Compiler reset discards semantic/generated IR, lexical storage and temporary strings while
retaining loaded source documents. The layout `Document` owns durable compiler storage,
document-local declaration IDs, immutable payload versions and a replaceable semantic graph.
Its transaction history stores forward changes and an applied cursor; undo/redo replay the
selected prefix. Handles belong to their storage owner. UI state that survives graph replacement
retains declaration identities and resolves them in the current document.

Material compilation uses the compiler's strings. A deserialized `CompiledMaterial` owns its
own strings so runtime consumers need no frontend. Its serialized source locations and node
order are preserved. Final output buffers, formatted diagnostics, filesystem boundary values
and temporary output formatting buffers remain ordinary strings where independent
ownership is useful; retained compiler nodes and declarations store handles.

## Ordinary value records

`settings-schema` declares a separate field adapter for an existing C++ container. It does not
generate or change that container, and its output has no S7 dependency:

```lisp
(module simulation_fields
  :header "ioj/s7/simulation_fields.h"
  :namespace ioj::s7::simulation_fields
  (settings-schema Fields
    :target ioj::sim::SimulationSettings
    :target-header "ioj/sim/levels/simulation_settings.h"
    (field "fighters.speed" "fighters.speed")
    (field "tick.tick-rate" "clock.tick_rate")))
```

This emits `FieldsField`, `try_parse_fields_field(path)` and
`visit_fields_field(target, field, callback)`. Parsing returns an optional enum; the visitor
requires a valid parsed enum and passes the actual member reference to a boolean callback.
The callback supplies scalar, enum or vector conversion at the loading boundary. Member types
and defaults remain owned by the C++ container. Field paths must be unique, and member paths
are dot-separated C++ identifiers. These adapters are metadata declarations without semantic
layout types; they reuse normal module assembly, dependency emission and source rendering.

Record members distinguish an omitted initializer from `:initializer ""` (C++ `{}`) and
`:initializer "3.f"` (C++ `{3.f}`). The string is the brace initializer's expression list;
it cannot contain statements or preprocessor directives. Initialization is retained in the
shared schema, resolved record members, and editable source, but does not affect layout analysis.
Record members retain their authored order and remain public aggregate fields.

`:comparison equality` and `:comparison three-way` opt into a defaulted const member operator.
`:comparison-noexcept true` preserves an explicitly noexcept declaration. Nothing is synthesized
when the comparison property is absent.

Records reuse typed `function` forms, including `:const`, `:noexcept`, `:constexpr`, and
`:nodiscard`. A body emits a member definition; an absent body emits a declaration implemented
in handwritten C++. Record methods do not support constructors, access control, templates, or
generated source definitions. Typed signatures participate in shared dependency and rename/move
handling. As with SoA functions, identifiers inside C++ expressions and bodies are authored text.

Signature type uses distinguish declarations from header definitions. Bodyless record methods may
use forward-declared value types; header bodies require complete by-value return and parameter
types. Pointer/reference uses use forward declarations where the generated C++ kind allows them.
Parameters with defaults conservatively require complete semantic types, including pointer and
reference parameters; default expressions are not analyzed. Other signature uses remain unchanged.
Across modules, bodyless record signatures emit these declarations in the target namespace before
the owning namespace, without adding a generated-header include. Explicit registered header
dependencies are retained. Header-defined by-value uses and record members still require the target
header; enums and aliases also retain their normal definition dependencies.
Aliases and enums precede their users, and complete-definition cycles are rejected across modules
as well as within a header. These emission dependencies do not reorder authored declarations.
Record and SoA owner/view functions share qualifier, return-type, parameter, overload and dependency
validation, with receiver-specific restrictions applied separately.

The simulation schema owns aggregate data, enums, and ordinary scalar aliases. Strong wrappers
such as `HealthIndex` remain C++ declarations because their constructors,
private representation or nested constants are outside this record model; external integer
registrations expose their domains and invalid sentinels. Telemetry `std::array` aliases also
remain handwritten: record fixed counts generate member arrays, which do not declare these aliases. Their physical facts can
still be supplied through the planner's existing ABI profile/probe system.

Lispb parses constrained declarative input and emits checked source assets; it is not a general Lisp
runtime or an escape hatch for arbitrary C++. Each DSL owns its grammar, semantic validation, and
stable output contract while sharing the parser and generation workflow.

`kernel` describes typed operations and native C++ output files. `slate` describes static
Slate trees and host integration. `material` describes material graphs and emits Unreal assets.
Generated outputs are checked in as ordinary repository source, with public headers in each
consumer's public include directory and private headers/sources beside their handwritten peers.
Modify the DSL input, run the matching CMake target, and review and commit the generated diff.

See [README.md](README.md) for entry points.

## Schema modules and declarations

A normal `(module name ...)` is one source and C++ output context, not a declaration family.
Its row in `NormalModuleSchemas` has one ordered range of `DeclarationHandle` values. Enums, integer
scalars, the six physical-representation declarations, packed values, records, ordinary and
tagged unions, SoAs, static tables, and facades can coexist.
The sequence is the source of truth for declaration indices and canonical source order.
Enum helper namespace, header/source paths, C++ namespace,
includes, and prelude remain module-wide policies. Settings modules remain special because they
describe generated settings integration rather than independent semantic types; umbrella modules
remain special because they aggregate existing outputs.

`ModuleHandle` selects a row in `NormalModuleSchemas`, `SettingsModuleSchemas`, or
`UmbrellaModuleSchemas` in the central declaration store.
The parser constructs the canonical declaration representation directly; validation, editing,
graph resolution, and generation consume it without a conversion pass.

The `TypeGraph` resolves normal declarations directly in sequence. Its `TypeIdentity` remains
origin, module name, C++ namespace, and declaration name; its `TypeId` values are per-resolution
handles, not persistent editor IDs. Registered external types still come from the separate
`types.lispb` input through the project's `:types` path.

C++ lowering is declaration-oriented. Each declaration contributes header/source nodes and
include requirements in a `DeclarationEmission`; a common assembler applies the module's
include order, prelude, namespace, and output envelopes. Explicit declaration-specific C++
operations remain visible in their lowerers. The CLI calls the target compiler library to
compile and publish targets; compiler selection is not embedded in argument parsing.
Emission places same-module record and union physical dependencies before their users, including
tagged-union discriminants. This leaves source declaration indices unchanged and does not order
declarations by semantic navigation relationships.

Declaration metadata is exhaustive: semantic-TypeGraph contribution, canonical source rendering,
and generated C++ names are each derived by visiting declaration handles. Generated-name ownership
includes public helper types such as SoA views and layout descriptions, so validation rejects
cross-declaration collisions before lowering a mixed output module.

`ioj::layout::Document` is the authoring boundary. Callers queue named edits in a `Transaction`
and inspect declarations through `DocumentQuery`. Instructions may temporarily break semantic
rules; commit validates the completed candidate once and publishes it atomically. Rejection
records diagnostics and leaves the published document and history unchanged. Project sources,
schema edits and layout variants share one history.

LispB owns canonical source rendering and comment attachment. The lexer records comments once;
`SourceComments` follows declaration versions and child renames. Saving renders complete affected
files, preserves comments and authored literal contents, and retains each file's line endings.
Whitespace and unchanged token spelling are not persistence contracts. No inverse-edit or
token-patching implementation remains in the schema library.

## Packed C++ values

Packed values store one unsigned fixed-width integer. The schema defines offsets, widths, and
bit order; C++ lowering uses explicit masks and shifts through `ioj::PackedField` and the
`packed_extract`, `packed_pack`, and `packed_insert` helpers in `ioj/packed_value.h`.
Each field exposes `<name>_field::{offset,bits,value_mask,mask}` instead of four separate
`<name>_offset`, `<name>_bits`, `<name>_value_mask`, and `<name>_mask` constants.
Storage checks share `valid_packed_storage`. `PackedField` also requires a value representation
wide enough for its bits: integral types, one-bit bool, or enums with unsigned underlying types.
Wider value types may represent narrower fields. Semantic ranges, enumerator membership, and
sentinel validation remain in generated code. Standard fixed-width integer spellings remain unchanged.

An explicit field-value constructor replaces `make(...)`. `Type::from_raw(raw)` replaces
the raw-storage constructor and preserves all bits without validation. Default construction
produces zero or the schema's invalid value when no field defaults are declared. Constructors and
mutable setters assert their input contracts; neither mutable nor immutable types expose
`try_make` or `try_set_*`. Immutable types have no setters. Invalid constructor/setter inputs
are programming errors, not recoverable failures. Storage layout, comparison, and serialization
are unchanged.

Packed fields accept numeric `:default` values, for example
`(field task std::uint8_t :bits 1 :default 0)`. Values use the field constructor's units:
integer/enum codes, fixed-point raw integers, or quantized/mini-float encodings. Explicit defaults
must fit the field's domain and width. When every non-reserved field has a default, default
construction packs those values with zero reserved bits; the result must not equal `:invalid-value`.
An incomplete explicit default set is valid authoring state but deletes the C++ default constructor.
Full-field constructors and `from_raw` remain usable. Removing every explicit default
restores implicit zero initialization, with `:invalid-value` taking precedence where declared.

## Integer scalar C++ emission

An integer scalar retains its own semantic identity, domain, named codes, and relationships.
Its one-of-N `:cpp-emission` policy is `none`, `constants`, `constants-with-names`, or `alias`.
`alias` requires an explicit supported integer `:cpp-type` whose range contains the scalar domain
and named codes; it needs no named codes and emits no constant helpers. Width does not infer a C++ type.

```lisp
(integer-scalar Health :signed false :minimum 0 :maximum 65535 :bit-width 16
  :cpp-type @native_uint16 :cpp-emission alias)
```

With `native_uint16` registered as `std::uint16_t` from `cstdint`, this emits
`using Health = std::uint16_t;` in the module namespace, with the normal include dependency.
Only alias emission supplies `IntegerScalarType` with a resolved representation reference and
dependency for physical layout analysis. Constants modes use `:cpp-type` only to type their
emitted constants; they leave the scalar purely semantic. All scalar emission modes share C++
type dependency resolution, including `CoreTypes.h` for raw Unreal integer spellings.
The emission policy remains schema/backend metadata. `using Health = std::uint16_t;` and
`using Armour = std::uint16_t;` describe distinct LispB semantic types but interchangeable C++ types.
Alias emission provides no strong typing or runtime domain checking.

## External scalar registry

`CompilerStorage::registered_types` owns registration columns linking `CppTypeId` to shared integer
domains or IEEE binary floating-point formats. `Manifest::types` maps identifier handles to
`RegisteredTypeId` values, so document snapshots copy bindings without copying metadata. This
metadata generates no C++ declaration. The graph retains external type identity and exposes it
to shared integer-domain consumers and planner inspection. Plain references with the same
normalized C++ spelling receive the same metadata; conflicting registrations are rejected.

Integer domains require `:signed` and `:bit-width` (1–64). Optional `:minimum` and `:maximum`
default to the full representable range; `(code name :value N :sentinel true)` can reserve named
values outside the live range. Validation applies the same range and code rules as declared
integer scalars. Floating-point formats are `ieee754-binary16`, `ieee754-binary32`, and
`ieee754-binary64`. These are semantic contracts; neither domain width nor format supplies an
ABI size or alignment. Integer representations accept external integer sources, while floating
point and opaque external types remain ineligible for integer-only representations.

External packed fields with `:bits auto` use the registered domain. Explicit-width external
fields (including `@` aliases) retain their field-local range, so existing narrow fields such
as a 24-bit `uint32` do not change when primitive metadata is included.

Registry `(include "relative/path.lispb")` forms load each canonical file once, reject cycles,
and contribute the entire closure to generator dependencies. The source store retains registry
and module roles and registration locations. Registry definitions remain source-authored,
with no parallel planner schema or structured editor.

## Native SoA ownership

An ordinary `struct` is the canonical logical SoA declaration. `SoaSchemas` retains its ordered
members, equivalent row type and mappings, relationships, and owner/view API. A nested member's
resolved type identifies another declared SoA. The semantic graph rejects invalid nested types
and cycles. The layout planner walks these same members across module boundaries to flatten the
physical columns; logical access retains the groups. Vector and rotator containers use this path
with explicit equivalent-value mappings and authored mathematical functions.

Every dynamic declaration emits one owner named after the schema, `NameView`, `NameConstView`,
resolved views, and `NameLayout`. Explicit view names can replace the defaults. `:layout-only true`
supplies a reusable nested group without an owner or standalone views. Such types have no physical
C++ spelling and cannot be record members or function parameter types. They remain valid logical
relationship targets and nested groups.

`NameLayout` derives from `ColumnLayouts<Ts...>` in physical leaf order and supplies named
integer constants for column indices. Ordinary function parameters select columns;
`make_column_layouts()` computes one constant metadata array without predecessor pointers or
constructor chains. Linear offsets use direct arithmetic; capacity-dependent alignment scans
only the nonlinear suffix. Layout metadata is freely copyable and adds no state to
owners or views. The existing layout uses 64 rows per block, a
192-byte inter-column gap, and 64-byte minimum alignment. `LayoutCursor` materializes typed column
pointers in one linear pass from the generated element types. The conservative capacity bound
and exact byte calculation retain overflow checks. C++ verifies element alignment, trivial
copyability, and construction requirements; nontrivial owning element types are rejected.

The owner holds one storage state and one non-owning PMR resource pointer. Its default constructor
selects `std::pmr::get_default_resource()`; the explicit constructor accepts a non-null
`std::pmr::memory_resource&`. Persistent and frame-local allocations use the same owner. The
resource must outlive it. Allocation and release pass the exact layout size and alignment.
Move construction adopts the source resource. Move assignment retains the destination resource,
transferring storage when resources compare equal and copying columns otherwise. Copy construction
and assignment are deleted.

`StorageOperations` coordinates lifetime, pointer access, copying, growth and removal using
generated, typed column pointers. Each schema declares one named `DataPointers<Byte>` aggregate,
shared by its owner and resolved views. These aggregates are initialized by
`ColumnLayouts::make_data()` expanding the element types in physical order. Aggregate expansion
shares pointer offsetting and default construction. One generated `Schema::with_columns()`
maps named source accessors into physical order for source pointers and column iteration.
It never stores independently resizable columns. One row count and capacity govern growth,
insertion, removal, copying, and permutation; equal-column-size validation is unnecessary inside
the owner. Shared transfers expand matching pointer aggregates in physical column order;
permutation expands the same aggregate into `ColumnPermutation`.
Row indices and counts use
`std::uint32_t`; allocation
bytes use `std::size_t`. Span lengths are checked before conversion to the row-count type.

Append and copy accept owners, views, and structural sources with a matching `soa_schema` tag,
`num()`, scalar column accessors, and nested `view_member()` accessors. External sources must
provide `validate()` for independently supplied column lengths and ranges. Owners guarantee
consistent columns structurally; resolved views check incoming columns in their constructors.
Compact views retain range validation against the owner. Append validates the range and captures
source pointers before growth, then keeps the original allocation alive until all copies finish,
allowing owner and view self-append across reallocation. External source storage must remain valid
during destination growth. Overlapping copy uses the per-column
move path; allocation growth and non-overlapping append use the copy path.

## Shared schema views

Each schema has one snake-case namespace containing `Layout`, `Schema`,
`DataPointers`, `View`, `ConstView`, `ResolvedView` and `ResolvedConstView`. Owners retain their declared names
and expose `View` and `ConstView` aliases. Nested accessors return the child's shared view;
the generator does not create types for individual occurrences of a nested schema.

`CompactViewBase<Const>` stores a state pointer, offset and count for mixed-schema owners.
`StridedViewBase<T, Const>` stores the first column address, stride and count for homogeneous
schemas, including nested homogeneous groups. The existing layout guarantees evenly spaced,
separate arrays of the same element type. Float groups occupy 16 bytes on native x64; types whose
stride cannot safely fit the compact encoding use a `std::size_t` byte stride and occupy 24 bytes.
Mixed nested groups use the child's resolved view. Scalar accessors return `std::span`.

`ResolvedViewBase` owns the schema's pointer aggregate and one row count. The layout's existing
type pack supplies its pointer and span constructor signatures. Pack expansion shares validation,
mutable-to-const conversion, slicing and indexed column access without repeating physical fields.
Named accessors remain generated to preserve owner borrowing and representation-specific nested
view return types; compact and resolved accessors use one generator implementation.

All views remain trivially copyable and inherit `ViewOperations` for borrowing and slice convenience
functions. Schema-specific accessors and custom methods are emitted once per representation.
Mutable views convert only to their matching const view. Owner borrowing requires an lvalue and
checks the initial range; slicing checks its subrange. Accessors trust these preconditions.

Calling `.resolve()` materializes a pointer per physical column plus one count. Resolve once
outside a hot loop when repeated layout calculations would be wasteful. Resolved views preserve
physical column order through the layout's type pack; compact and strided bases share the
construction, using the owner's layout and the borrowed stride respectively. Resolved views retain
logical nesting and the declared equivalent-value and custom view API. A dynamic nested group's
resolved view uses the child's canonical resolved type, including layout-only groups.
Resolving an already resolved view returns that view. External span constructors
check equal lengths, non-null pointers for nonempty columns, and row-count conversion.

All representations borrow storage. The owner and resource must outlive them. Reallocation, owner
moves, reset, and mutation that removes or reorders borrowed rows require reacquiring views.
Resolved pointers do not follow reallocation. View validation checks meaningful source and range
preconditions; it does not add owner column-size invariants or detect stale borrows.

## Logical SoA API

`soa_api` derives function signatures, receiver selection, aliases, and logical column expressions
from the canonical schema. `:equivalent-type` supplies a row value, constructed from columns in
declaration order unless `:equivalent-constructor` names an explicit constructor function. Per-member
`:equivalent-member` mappings select input fields; `:equivalent-accessor true` selects a member call.
This supports scalar and equivalent-value insertion, assignment, and retrieval without recognizing
vector semantics from column names or C++ spelling.

`function`, `view-function`, and `mutable-view-function` select owner, read-only-view, and mutable-view
receivers. Read-only view functions are const. Bodies emit inline unless
`:definition-in-source true` requests an out-of-line definition, which requires a source output
and is forbidden for templates. Bodyless functions declare handwritten implementations. Const
qualification on a mutable view does not make its pointees const.

An owner function can use `:append-columns (column ...)` instead of a C++ body. Each column names
a direct array member and receives the corresponding by-value parameter. The new row is default
initialized, including unlisted and nested columns, and its index is returned as the declared row
index or ID type. An empty list appends a fully defaulted row. For example:

```lisp
(function append WorkloadId
  (parameter owner AnalysisId)
  (parameter count std::uint64_t)
  :append-columns (analysis element_count))
```

The usual `:definition-in-source` option applies. Passing arguments by value preserves inputs
borrowed from the owner when growth reallocates its storage.

Authored bodies remain raw C++. `$column(values)` and `$column(positions.xs)` expand through logical
column accessors. Expansion is textual, including inside comments and literals; other identifiers
remain unchanged. Custom functions and using declarations must be valid for their declared receiver.
Generated field masks derive from the same logical member annotations. Export specifiers apply to
the public owner and views.

Generated headers are checked in with their consumers. Regenerate production and compile fixtures
after changes to schema, layout planning, or emission. The `codegen` and `native-soa` checks cover
schema rejection, nested composition, equivalent values, external sources, layout, growth, aliasing,
move/resource behavior, and view bounds. Native simulation tests cover the production consumers.
Fixture schemas and generated code live in `tests/native_soa`; production-schema benchmarks live
in `benchmarks/soa`. PMR contract tests cover resources independently of the container tests.
The SoA compiler and generated owners require no Unreal headers or libraries; separate enum,
settings, Slate and material features retain their own integration contracts.
