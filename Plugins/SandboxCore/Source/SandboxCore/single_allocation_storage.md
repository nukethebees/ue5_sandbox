# Generated single-allocation SoA

The LispB `single-allocation` declaration selects single-allocation ownership:

```lisp
(single-allocation SingleEntityData)
```

Use `:storage both` when a schema also needs an ordinary generated owner (TArray-backed in Unreal and vector-backed in native builds). Schemas without a single-allocation declaration default to vector storage; `:storage vector` and `:storage single-allocation` can make the choice explicit. SandboxCore provides the Unreal runtime. Native and Unreal owners share the layout and compact-view implementation under `native/core`.

## Ownership and layout

An owner stores one allocation pointer, one `int32` size and one `int32` capacity: 16 bytes on Win64. Each flattened leaf occupies a contiguous column in the allocation. Nested schemas flatten in depth-first declaration order from the resolved type graph.

Capacity is a multiple of 64. Each column is aligned to `max(64, alignof(T))`, followed by its capacity elements and a **fixed 192-byte gap** before aligning the next column. There is no trailing gap. Stronger alignment can increase the effective gap. The allocation uses the maximum leaf alignment. This policy applies to both backends and every single-owner allocator variant, with no capacity-dependent exceptions.

Generated constexpr offset functions apply this sequential layout using `capacity / 64`; the gap never scales with that count. `layout_bytes(blocks)` returns the exact extent, including zero for empty storage. `capacity_block_bound` is a conservative arithmetic bound used to reject overflowing capacities, not the actual allocation size. Column pointers are resolved when accessing/materializing views, outside entity loops. Tests compare the generated offsets against an independent sequential reference through 256-byte alignment.

Reserve rounds to the granularity. Append growth uses geometric slack before rounding. Growth allocates one new block, bulk-copies each live column, then releases the old block. Allocation-size and row-count arithmetic are checked. Owners remain move-only; reset and removal retain capacity.

`max_capacity` is a schema-specific, conservative limit bounded by `int32` row counts and `PTRDIFF_MAX` allocation bytes, rounded down to 64 rows. Requests beyond it fail before allocation. Public generated layout functions accept a validated capacity-block count; callers must not pass arbitrary byte-sized integers. Column types may repeat: columns are identified by their declaration position and field path, not by type. Empty schemas are rejected by the generator. Over-alignment is supported provided the alignment fits the allocator's 32-bit argument.

Moving transfers the allocation without allocating and leaves the source with zero size and capacity. Move assignment first releases the destination's previous block. Reset only clears the logical size; destruction releases the retained block.

Leaves must be non-cv, non-array object types that are trivially copyable, trivially copy constructible, trivially destructible and nothrow default constructible. Location-dependent invariants requiring relocation fixups are unsupported. Byte-array placement construction establishes the supported implicit lifetimes without initializing the allocation. Defaulted additions use the backend's normal default/value construction policy.

## Compact views

`View` and `ConstView` are separate named structs, each storing an owner-state pointer, row offset and row count. Both are 16 bytes and trivially copyable. Mutable handles implicitly convert to const handles, but not the reverse. Allocator variants share view types for the same schema.

```cpp
auto rows = data.get_view();
auto healths = rows.healths();
auto locations = rows.view_locations();
auto xs = locations.xs();
auto selected = rows.slice(32, 64);
auto readonly = selected.get_const_view();
```

Views capture a range, not a growing count. A top-level compact handle re-resolves columns through owner state, so it can survive growth while its owner stays in place and its captured range remains valid. Extracted spans, ordinary views and vector views retain allocation pointers and become stale after growth. Do not use outstanding views across either owner move or after destruction. Move construction leaves handles referring to the moved-from state; move assignment replaces the destination's state, so its old handles can silently observe the moved-in allocation and even pass validation. This invalidation is a logical API contract, not runtime-tracked. Shrinking/removal can invalidate a range or change which entities it denotes; views are not stable entity references.

Owner borrowing functions require an lvalue, preventing accidental views from temporary owners. Temporary non-owning views can still be sliced. Explicit pointer-based view construction remains the caller's responsibility: the owner and backing storage must outlive every use.

The explicit `:vector-components (xs ys)` or `(xs ys zs)` annotation marks the special Cartesian compact-vector shape. LispB requires exactly those ordered arrays with a common resolved element type; the generated C++ view additionally requires that type to be arithmetic and non-volatile. The generator does not infer vector meaning from member names. Their Unreal names are `ml::soa::Vector2View<T>`, `Vector2ConstView<T>`, `Vector3View<T>` and `Vector3ConstView<T>` (available through `SandboxCore/single_allocation/vector_views.h`). The native backend exposes the corresponding names in `ml::native_soa`. Both use the same implementation; Unreal component accessors return TArrayViews and native accessors return standard spans.

Each vector view is 16 bytes: a first-component pointer, a 32-bit byte stride, and a 32-bit row count. The generated layout guarantees equally spaced component columns. `slice`, `left` and `right` advance the first pointer while retaining the component stride, including for empty end slices. Mutable views convert to const views, but not the reverse. The vector view has no owner pointer or field-specific layout type.

For example, `view_locations()` and `view_velocities()` both return `ml::soa::Vector3View<float>`. Resolve `xs()`, `ys()` and `zs()` outside hot loops. Other nested shapes receive generated compact views rooted in the parent's owner state, so they can survive growth under the same range contract as the parent view.

Mutable vector views remain writable when the view object itself is const, like an ordinary span; const-view aliases expose only const elements. Constructor and slice range checks use the backend's normal failure mechanism. Direct construction requires sufficiently large, equally spaced component arrays and a byte stride that preserves element alignment and fits in uint32.

Algorithms normally take compact views by value and extract only the spans they use. Generated `each_column` / `apply_arrays` access the flattened leaves directly. The shared `validate_compact_view` contract checks size and trivial copyability with separate diagnostics.

## Bulk insertion

```cpp
auto first = destination.append_from(source.get_const_view());
destination.append_from(source.get_const_view().slice(offset, count));
destination.append_from(frame_source);
destination.append_from(destination.get_const_view()); // self-append, including growth
destination.append_from(destination.slice(offset, count));
destination.add_uninitialised(count);
destination.add_defaulted(count);
```

`append_from` accepts a compact view or an independent-column source exposing `num()`, `validate()`,
scalar span accessors and nested `view_<member>()` accessors. It returns the first inserted row index.
The generated `accepts_source` constraint checks the required leaf types. Independent producers
validate their own column lengths and must keep their storage alive through the call.

Append checks the source range and final size, grows at most once, resolves source accessors after
growth, bulk-copies each leaf and publishes size once. Whole and sliced compact self-append need no
temporary container: the source still refers to the owner's updated state. Independent sources must
not contain cached spans into the destination allocation. Empty append is a no-op, and copied
values are independent of their source.

`add_uninitialised` creates logical rows without writing their values; callers must initialize them before reading. `add_defaulted` initializes the new rows. AoS input ranges, arbitrary aggregates of unrelated spans, non-trivial relocation and deep-copy constructors are outside this API.

## Bulk swap removal

```cpp
data.remove_at_swap(index, count);
data.remove_at_swap(TConstArrayView<int32>{descending_indices});
// std::span<int32 const> is also accepted, including in the native backend.
```

Index lists must be strictly descending, unique and refer to the original rows. The entire list is validated before copying. No sorting, deduplication or scratch allocation is performed.

Holes below the final size are filled by surviving tail rows in ascending row order. Adjacent moves are coalesced into block copies and applied across columns. Descending input does not reverse the copied tail. For example, removing `(2, 3)` or indices `[4, 3, 2]` from `[0,1,2,3,4,5,6,7,8,9]` produces `[0,1,7,8,9,5,6]`. This matches TArray's contiguous-range removal, not repeated one-row removal at one index. Capacity stays unchanged.

For scattered indices this can also differ from repeatedly removing one row in descending order: removing `[7,5,1]` from `[0..9]` produces `[0,8,2,3,4,9,6]`. Consumers must use the bulk operation's movement ordering when maintaining external row-index mappings.

## Allocator integration

Unreal single owners default to `ml::soa_storage::MimallocStorageAllocator`. SandboxCore links a
private static mimalloc implementation built from the repository's vendored source. Every
externally visible mimalloc symbol is prefixed with `sbx_`, and game code reaches it only through
the `sbx::memory` wrapper. Unreal's global allocator is unchanged and no allocator override or CRT
redirection is enabled. This integration requires Win64 x64 and the dynamic release CRT, as
enforced by the module rules. Restart the Editor after rebuilding SandboxCore; reloading a module
while its private allocator still owns live storage is unsupported.

Allocator variants select a type providing `allocate(bytes, alignment)` and `free(data)`. The
native backend retains its standard/mimalloc configuration choices and uses the same private
allocator library without linking Unreal.

## Validation and measurement

The test suites cover compact view sizes and trivial copyability, mutable/const conversion, rejection of temporary-owner borrowing, vector strides and invalid slices, aligned bulk copies, self-append, empty inputs, generated layout limits, overflow, descending removal subsets and invalid indices. A generated owner with a counting allocator checks single-block allocation, growth, reset, move assignment over an owning destination, and balanced destruction without allocating at the arithmetic limit.

Run correctness validation from the repository root:

```powershell
cmake --workflow --preset native-tests
cmake --workflow --preset native-soa
cmake --build --preset generate-code
cmake --build out/build/native --target check-generated-code
```

`native-tests` is the normal native correctness suite. `native-soa` builds and runs both native
SoA allocation variants (`native-soa-tests` and `native-soa-tests-mimalloc`) and its benchmark
smoke checks; it is not a timed performance comparison. `generate-code` and
`check-generated-code` validate committed generated output. Allocation counting uses the compile
fixture's allocator adapter. Small schemas in `native/lispb/native_soa` cover odd-sized and
32/64/256-byte-aligned leaves, repeated nested vectors, layout limits, and bulk operations. The
native SoA suites exercise actual standard and mimalloc storage and alignment.

For Unreal-dependent behavior, build the Editor and run the existing CQTest/Unreal Automation
unit suite. The private SandboxCoreEngineTests fixture covers compact handles across growth,
self-append, nested access, and move assignment:

```powershell
cmake --workflow --preset debug-game
ctest --test-dir out/build/debug-game -R '^Sandbox\.UnitTests$' --output-on-failure
```

The native standard-allocation suite also passes Clang 21 AddressSanitizer on Win64. The generated
clang-cl presets ending in `-asan` enable the instrumentation and place the required Clang runtime
beside the native executables. To check only the standard allocation path, run:

```powershell
cmake --preset win-x64-clangcl-release-asan
cmake --build --preset win-x64-clangcl-release-asan --target native-soa-tests
ctest --preset win-x64-clangcl-release-asan -R '^native-soa-tests$'
```

This checks the standard allocation path, not mimalloc internals. UBSan has not been run.

The `native-soa-benchmarks` executable measures production fighter, laser, spinner, and laser-hit storage. Its production report targets remain available through CMake; see [Benchmarks](../../../../docs/benchmarks.md) for the supported workflow.
