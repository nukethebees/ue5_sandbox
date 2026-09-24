# Lispb architecture

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

The LispB `soa` declaration is validated before lowering. A single-allocation owner is a
declaration on a normal SoA; nested schemas and flattened column names are checked during semantic
validation. The explicit `:vector-components (xs ys)` or `(xs ys zs)` annotation requires
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
`StorageOperations`; it contains explicit typed per-column mutations and an alias guard. Native
and Unreal support provide `copy_n`, `default_construct_n`, `source_data`, and `any_column`,
so generated code neither repeats byte-count arithmetic nor emits an alias OR per column.
The owner has no generated `FooStorage` intermediate. Public named mutable and const compact
views are thin wrappers over one `FooSingleViewImpl<Const>` accessor implementation. They hold a
stable pointer to owner state plus offset/count, resolve pointers lazily, and remain 16-byte
trivially copyable handles. A retained compact handle survives allocation growth while the owner
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
