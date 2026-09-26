# Lispb architecture

## Ordinary value records

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

The simulation schema owns aggregate data, enums, and ordinary scalar aliases. Strong wrappers
such as `HealthIndex` and `EntityOwnerId` remain C++ declarations because their constructors,
private representation or nested constants are outside this record model; external integer
registrations expose their domains and invalid sentinels. Telemetry `std::array` aliases also
remain handwritten: record fixed counts generate member arrays, while homogeneous layouts emit
different public C++ types. Neither faithfully declares these aliases. Their physical facts can
still be supplied through the planner's existing ABI profile/probe system.

Lispb parses constrained declarative input and emits checked source assets; it is not a general Lisp
runtime or an escape hatch for arbitrary C++. Each DSL owns its grammar, semantic validation, and
stable output contract while sharing the parser and generation workflow.

`kernel` describes typed SandboxCore operations and C++ emission profiles. `slate` describes static
Slate trees and host integration. `material` describes material graphs and emits Unreal assets.
Generated outputs remain repository-owned source: modify the DSL input, run the matching CMake
target, review the generated diff, and keep hand-written integration at the boundary.

See [README.md](README.md) for entry points.

## Schema modules and declarations

A normal `(module name ...)` is one source and C++ output context, not a declaration family.
Its `NormalModuleSchema` has one ordered `DeclarationSchema` variant sequence. Enums, integer
scalars, the six physical-representation declarations, packed values, records, ordinary and
tagged unions, SoAs, vector SoAs, homogeneous layouts, static tables, and facades can coexist.
The sequence is the source of truth for declaration indices and source-preserving editing.
Backend and allocator selection, enum helper namespace, header/source paths, C++ namespace,
includes, and prelude remain module-wide policies. Settings modules remain special because they
describe generated settings integration rather than independent semantic types; umbrella modules
remain special because they aggregate existing outputs.

`ModuleSchema` contains only `NormalModuleSchema`, `SettingsModuleSchema`, and
`UmbrellaModuleSchema`. Declaration-family module types and parser heads no longer exist.
The parser constructs the canonical declaration representation directly; validation, editing,
graph resolution, and generation consume it without a conversion pass.

The `TypeGraph` resolves normal declarations directly in sequence. Its `TypeIdentity` remains
origin, module name, C++ namespace, and declaration name; its `TypeId` values are per-resolution
handles, not persistent editor IDs. Registered external types still come from the separate
`types.lispb` input. The special `:types` project path is retained until registry declarations
and source-preserving project edits can be migrated together.

C++ lowering is declaration-oriented. Each declaration contributes header/source nodes and
include requirements in a `DeclarationEmission`; a common assembler applies the module's
include order, prelude, namespace, and output envelopes. Explicit declaration-specific C++
operations remain visible in their lowerers. The CLI calls the target compiler library to
compile and publish targets; compiler selection is not embedded in argument parsing.
Emission places same-module record and union physical dependencies before their users, including
tagged-union discriminants. This leaves source declaration indices unchanged and does not order
declarations by semantic navigation relationships.

Declaration metadata is exhaustive: semantic-TypeGraph contribution, canonical source rendering,
and generated C++ names are each derived by visiting `DeclarationSchema`. Generated-name ownership
includes public helper types such as SoA views and homogeneous storage/view traits, so validation
can reject cross-declaration collisions before lowering a heterogeneous output module.
SoA array-allocator variants use one validation and expansion policy; they are limited to plain
dynamic Unreal SoAs and their prefixed generated
types are checked before lowering.

`EditableSchemaDocument` owns the manifest draft, source-file and declaration ranges, stable
`DeclarationId` values, tombstones, undo/redo, and the last valid resolved graph. Normal-module
declarations move as one variant element, including across declaration families. Successful
edits replace the graph and adjust locations; rejected edits restore the previous manifest,
graph, source ownership, and revision. Source-backed module deletion and restoration retain
their original ranges, including comments and unrelated text.

## Packed C++ values

Packed values store one unsigned fixed-width integer. The schema defines offsets, widths, and
bit order; C++ lowering uses explicit masks and shifts through `ml::PackedField` and the
`packed_extract`, `packed_pack`, and `packed_insert` helpers in `sandbox/core/packed_value.h`.
Each field exposes `<name>_field::{offset,bits,value_mask,mask}` instead of four separate
`<name>_offset`, `<name>_bits`, `<name>_value_mask`, and `<name>_mask` constants.
Storage checks share `valid_packed_storage`. `PackedField` also requires a value representation
wide enough for its bits: integral types, one-bit bool, or enums with unsigned underlying types.
Wider value types may represent narrower fields. Semantic ranges, enumerator membership, and
sentinel validation remain in generated code. Standard fixed-width integer spellings remain unchanged.

An explicit field-value constructor replaces `make(...)`. `Type::from_raw(raw)` replaces
the raw-storage constructor and preserves all bits without validation. Default construction
produces zero or the schema's invalid value when no field defaults are declared. Mutable types retain `try_make` and setters;
immutable types expose neither. These are source API migrations; storage layout, comparison,
serialization, and value validation are unchanged.

Packed fields accept numeric `:default` values, for example
`(field task std::uint8_t :bits 1 :default 0)`. Values use the field constructor's units:
integer/enum codes, fixed-point raw integers, or quantized/mini-float encodings. Explicit defaults
must fit the field's domain and width. When every non-reserved field has a default, default
construction packs those values with zero reserved bits; the result must not equal `:invalid-value`.
An incomplete explicit default set is valid authoring state but deletes the C++ default constructor.
Full-field constructors, `try_make`, and `from_raw` remain usable. Removing every explicit default
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

`Manifest::types` owns `RegisteredTypeSchema`: the existing C++ spelling/includes/operations and
an optional integer domain or IEEE binary floating-point format. This metadata generates no C++
declaration. The graph retains `ExternalType` identity and exposes the resolved scalar metadata
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
and contribute the entire closure to generator dependencies. `EditableSchemaDocument` tracks
registry and module source roles explicitly, including registration locations. Source navigation,
save/reload and clone use this closure; clone rewrites include paths to its copied sources.
Registry definitions remain source-authored, with no parallel planner schema or structured editor.

## Single-allocation SoA

The LispB `struct` declaration selects `:storage vector`, `single-allocation`, or `both`.
Without an explicit policy, a `(single-allocation OwnerName)` declaration selects only that
owner; other schemas emit vector-backed owners. `both` explicitly requests both representations.
Nested schemas and flattened column names are checked during semantic validation.
The explicit `:vector-components (xs ys)` or `(xs ys zs)` annotation requires
exactly those ordered arrays with a common resolved element type. The generated compact vector
view additionally compiler-checks that the C++ element type is arithmetic and non-volatile.
The resolved `TypeGraph` records the vector meaning and checks a declared vector equivalent's
components and element type when one exists. The single-allocation planner walks
resolved graph columns for physical order and C++ types; emission receives those typed columns
and does not infer vector semantics. C++ ABI facts such as `sizeof`, `alignof`, trivial
copyability, and default construction remain compiler-checked.

The generated `FooSingleLayout` is metadata, not an owner base. Its chained `ColumnLayout<T>`
constants determine random-access offsets, exact allocation size, and alignment. All layouts and
`CompactViewState` use the library-owned `LayoutPolicy` (64 rows/block, 192-byte inter-column gap,
64-byte minimum alignment). A small `LayoutCursor` materializes typed column pointers in one
linear pass using the same placement primitives as random-access offsets; independent chained
`Column.offset(blocks)` calls would be correct but make large production layouts expensive to
compile and materialize. `capacity_block_bound` is deliberately conservative while
`layout_bytes` remains exact. The cursor and chained offsets are tested against each other,
including same-type adjacent and over-aligned columns.

The generated owner directly holds `StorageState` and inherits small generic
`StorageOperations`; it contains explicit typed per-column mutations. Native and Unreal support
provide `copy_n`, `default_construct_n`, and `source_data`. Append accepts a structural source
with `num()`, `validate()`, scalar column accessors, and `view_member()` nested accessors.
It validates the source range before growing the destination, then resolves source columns
after growth. Compact self-append therefore survives reallocation. Independent sources, such as
frame-backed producers, expose their own columns directly and validate their column lengths.
Their storage must remain valid through destination growth.
The owner has no generated `FooStorage` intermediate. Public named mutable and const compact
views are thin wrappers over one `FooSingleViewImpl<Const>` accessor implementation. They hold a
stable pointer to owner state plus offset/count, resolve pointers lazily, and remain 16-byte
trivially copyable handles, checked with the shared `validate_compact_view` contract. Non-vector
nested views share the same owner state and range. Column iteration calls individual accessors;
there is no schema-wide `columns()` conversion. A retained compact handle survives allocation growth while the owner
stays in place and its range remains valid; materialized spans and vector views do not. Moving an
owner is outside the view lifetime contract: move construction leaves handles referring to the
moved-from state, while move assignment replaces the destination's state and can make its old
handles observe the moved-in allocation without failing validation. Do not use outstanding views
across either move or after destruction; this is a logical contract, not runtime-tracked
invalidation. Owner borrowing methods constrain explicit-object parameters to lvalues, including
const lvalues; temporary owners cannot yield views.

Per-column generated code is intentional and inspectable. Do not replace it with a universal
storage template or variadic copy mechanism. Native and Unreal retain their distinct allocation
and ordinary-view APIs where those differences are real. Keep generated headers checked in and
regenerate both the compile fixture and production output after changing schema, planning, or
emission. Focused `codegen` and `native-soa` workflows cover rejection, layout, aliasing, growth,
view lifetime, and allocator variants; `native-tests` covers production simulation consumers.

### Logical API and storage policy

`SoaSchema` is the logical declaration. The type graph resolves its columns and relationships
before vector and single-allocation lowering split. Both lowerers use `soa_api` for function
signatures, receiver selection, aliases, and logical column expressions. A compact owner never
needs an ordinary owner or aggregate view to implement this API.

Every `SoaSchema` field has an explicit role:

| Field | Classification | Contract |
| --- | --- | --- |
| `name` | A: logical | Stable schema identity, independent of the owner name. |
| `members` | A: logical | Ordered logical columns, nesting, relationships and mask annotations. |
| `view_name`, `const_view_name` | D: naming decision | Name the sole compact pair for single-only; name vector views with vector or both. Compact names with both remain `NameSingleView` / `NameSingleConstView`. Layout-only has no standalone pair to name. |
| `operations` | A: API | Selects public storage mutations independently for each requested owner. |
| `export_specifier` | A: API | Applies to the schema's public owner and named mutable/const views. |
| `functions` | A: API | Owner functions, emitted on every requested owner, including allocator variants. |
| `const_view_functions` | A: API | Read-only functions on both mutable and const views. |
| `mutable_view_functions` | A: API | Functions on mutable views only. |
| `using_declarations` | A: API | Declarations in every canonical owner and view. Raw C++ must be valid in those receivers. |
| `equivalent_type` | A: logical/API | Semantic equivalent row type; `equivalent_type` alias and value-returning `operator[]` aggregate-initialize it from columns in declaration order. |
| `copy_element_memberwise` | B: vector | Chooses direct element assignment instead of container copying for ordinary nested storage. Compact leaves already require trivial copying, so single-only rejects it. |
| `layout_only` | D: logical layout decision | No physical owner or standalone views. Supplies columns and view API to nested compact occurrences. Rejects owner functions, operations, standalone view names and ownership settings. |
| `fixed` | B: vector | Additional fixed/container ownership using the ordinary storage abstraction; incompatible with single-only. |
| `single_allocation` | C: single allocation | Physical owner name; required for single-allocation or both, forbidden with vector. |
| `array_allocator` | B: vector | Unreal TArray allocation policy; rejected on single-only and by the standard-library vector backend. |
| `single_allocation_variants` | C: single allocation | Additional owners sharing the root compact view/layout family. |
| `single_allocation_allocator` | C: single allocation | Allocation policy for this owner; requires a single-allocation declaration. |
| `field_mask_name`, `field_enum_name` | A: logical/API | Generated once from the logical columns, before either owner backend, including layout-only schemas. |
| `vector_components` | A: logical | Component semantics and validation. Runtime vector-view substitution is permitted only when it preserves the entire declared view API. |
| `storage` | D: policy decision | Explicit physical representations. Default is single-only with a single-allocation declaration, vector otherwise; never implicit both. |

`function`, `view-function`, and `mutable-view-function` forms select owner, read-only-view,
and mutable-view receivers respectively. Read-only view functions are const. A body is emitted
inline unless `:definition-in-source true` requests an out-of-line definition; this requires a
source output and is forbidden for templates. A bodyless function remains an explicit C++
declaration for a handwritten definition. Mutable-view membership controls pointee mutation;
const qualification on a mutable view itself does not make its columns const.

Function bodies remain raw C++. LispB expands explicit `$column(values)` and
`$column(positions.xs)` expressions through logical member access. The same expression becomes
an ordinary column member or a compact accessor, with owner access through `get_view()`.
The expansion is textual (including inside raw C++ comments and literals); other identifiers
are untouched. Storage-specific handwritten bodies remain storage-specific C++, and `both`
requires such bodies to compile against both receivers. LispB does not guess how arbitrary C++
identifiers, containers, macros, or local variables should be rewritten. Custom nested view APIs
use generated state-relative compact views rather than losing functions through a vector alias.

`:operations` selects `reset`, `reserve`, `set-num`, `add-uninitialised`, `add-defaulted`,
`remove-at-swap`, `copy-element` (including `copy_elements`) and `append-from`. An omitted or
empty list exposes none of these. Construction, move/destruction, borrowing, size/capacity,
iteration and direct column access remain intrinsic. Ordinary owners also retain their existing
row `set`/`add` and permutation helpers, which are distinct from these bulk operations. Compact
owners privately inherit runtime implementations and expose only selected operations; internal
dependencies such as `set_num` growing defaulted rows do not expose additional public mutations.
Compact copy accepts the same structural sources as append and supports overlapping row ranges.
It uses a separate per-column `move_n` path (`memmove` / `FMemory::Memmove`), while append and
allocation growth retain non-overlapping `copy_n` (`memcpy` / `FMemory::Memcpy`).

The type graph retains the logical declaration identity while recording the single owner as
its physical C++ spelling for single-only storage. C++ type references and forward declarations
use that spelling. Layout-only nodes have an empty C++ spelling and cannot be used as physical
record members or function parameter types. They remain valid logical relationship targets and
nested schemas. Registered names, dependency edges and planner identity remain logical; ABI
facts for an owner are keyed by its physical spelling, never a deleted vector class name.
When a logical vector schema has a declared vector equivalent, the resolved model also retains
that vector declaration's `equivalent_constructor`. Compact row conversion uses it (for example,
`HMM_V3` for Handmade Math's union representation) instead of assuming aggregate construction.
