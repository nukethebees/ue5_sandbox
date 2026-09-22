# Lispb architecture

Lispb parses constrained declarative input and emits checked source assets; it is not a general Lisp
runtime or an escape hatch for arbitrary C++. Each DSL owns its grammar, semantic validation, and
stable output contract while sharing the parser and generation workflow.

`kernel` describes typed SandboxCore operations and C++ emission profiles. `slate` describes static
Slate trees and host integration. `material` describes material graphs and emits Unreal assets.
Generated outputs remain repository-owned source: modify the DSL input, run the matching CMake
target, review the generated diff, and keep hand-written integration at the boundary.

See [README.md](README.md) for entry points.

## Single-allocation SoA

The LispB `soa-module` schema is validated before lowering. A single-allocation owner is a
declaration on a normal SoA; nested schemas and flattened column names are checked during semantic
validation. A Cartesian nested view requires explicit `:vector-components (xs ys)` or
`(xs ys zs)`. The resolved `TypeGraph` records that meaning and checks a declared vector
equivalent's components and element type when one exists. The single-allocation planner walks
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
