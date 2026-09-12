# Generated single-allocation SoA

The `single_allocation` schema setting adds an opt-in owning representation alongside the normal TArray-backed generated owner:

```json
"single_allocation": {"name": "SingleEntityData"}
```

The runtime is part of SandboxCore. Comparison types remain in SbxCoreExperiments; production fighter storage has not been migrated. The former `experimental_single_allocation` setting has been renamed; repository manifests have been updated rather than maintaining two spellings.

## Ownership and layout

An owner stores one allocation pointer, one `int32` size and one `int32` capacity: 16 bytes on Win64. Each flattened leaf occupies a contiguous column in the allocation. Nested schemas flatten in depth-first declaration order using the fixed-SoA traversal.

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

Views capture a range, not a growing count. Growth, moving or destroying the owner invalidates borrowed views and extracted spans. Reacquire them after those operations; the top-level handle's internal owner-state pointer is not a promise of growth-following behavior. Shrinking/removal can invalidate a range or change which entities it denotes; views are not stable entity references.

Owner borrowing functions require an lvalue, preventing accidental views from temporary owners. Temporary non-owning views can still be sliced. Explicit pointer-based view construction remains the caller's responsibility: the owner and backing storage must outlive every use.

Nested schemas consisting of matching scalar `xs`/`ys` or `xs`/`ys`/`zs` columns use shared compact vector views. Their Unreal names are `ml::soa::Vector2View<T>`, `Vector2ConstView<T>`, `Vector3View<T>` and `Vector3ConstView<T>` (available through `SandboxCore/single_allocation/vector_views.h`). The native backend exposes the corresponding names in `ml::native_soa`. Both use the same implementation; Unreal component accessors return TArrayViews and native accessors return standard spans.

Each vector view is 16 bytes: a first-component pointer, a 32-bit byte stride, and a 32-bit row count. The generated layout guarantees equally spaced component columns. `slice`, `left` and `right` advance the first pointer while retaining the component stride, including for empty end slices. Mutable views convert to const views, but not the reverse. The vector view has no owner pointer or field-specific layout type.

For example, `view_locations()` and `view_velocities()` both return `ml::soa::Vector3View<float>`. Resolve `xs()`, `ys()` and `zs()` outside hot loops. `columns()` returns an aggregate with those component spans as fields; the top-level `rows.columns()` still materializes the original schema-level aggregate for existing algorithms. Other nested shapes retain their existing schema-level views.

Mutable vector views remain writable when the view object itself is const, like an ordinary span; const-view aliases expose only const elements. Constructor and slice range checks use the backend's normal failure mechanism. Direct construction requires sufficiently large, equally spaced component arrays and a byte stride that preserves element alignment and fits in uint32.

`view.columns()` explicitly materializes the existing larger aggregate of TArrayViews or native spans, including nested structure. This supports existing view-taking algorithms and pointer traversal. It does not change the compact view's size. Materialize outside entity loops; the aggregate has the same pointer invalidation rules as individual spans.

## Bulk insertion

```cpp
auto first = destination.append_from(source);
destination.append_from(source.slice(offset, count));
destination.append_from(destination); // self-append, including growth
destination.append_from(destination.slice(offset, count));
destination.add_uninitialised(count);
destination.add_defaulted(count);
```

`append_from` copies actual values from a compatible single-allocation owner or compact view and returns the first inserted row index. It checks final size, grows at most once, resolves source pointers after growth, bulk-copies each column and publishes size once. Whole and sliced self-append need no temporary container. Empty append is a no-op. Source values remain independent after copying.

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
FMemory comparison remains explicit. The native backend retains its standard/mimalloc
configuration choices and uses the same private allocator library without linking Unreal.

## Validation and measurement

The test suites cover compact view sizes and trivial copyability, mutable/const conversion, rejection of temporary-owner borrowing, vector strides and invalid slices, aligned bulk copies, self-append, empty inputs, generated layout limits, overflow, descending removal subsets and invalid indices. A generated owner with a counting allocator checks single-block allocation, growth, reset, move assignment over an owning destination, and balanced destruction without allocating at the arithmetic limit.

Run correctness validation from the repository root:

```powershell
cmake --workflow --preset codegen
cmake --workflow --preset native-soa
cmake --workflow --preset debug-game
cmake --build --preset debug-game --target core-tests
ctest --preset debug-game-unit-tests -R '^SandboxCore\.'
```

The native workflow includes benchmark pipeline smoke checks, not a timed comparison. The benchmark commands and reports remain separate. Allocation counting uses the compile fixture's allocator adapter; actual mimalloc storage and alignment are exercised separately by the native and Unreal suites.

The native standard-allocation suite also passes Clang 21 AddressSanitizer on Win64. For a separate local build, configure the `native-soa` preset with a different `-B` directory, `-DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer"` and `-DCMAKE_CXX_SCAN_FOR_MODULES=OFF`. Build only `native-soa-tests`, then run CTest with `-R '^native-soa-tests$'`. Make LLVM's `clang_rt.asan_dynamic-x86_64.dll` available beside the generated tools and test executable in `native/lispb/native_soa` before building; the instrumented generator runs during the build. This checks the standard allocation path, not mimalloc internals. UBSan has not been run.

The [comparison README](../SbxCoreExperiments/README.md) documents benchmark commands; the [investigation report](../SbxCoreExperiments/INVESTIGATION.md) preserves earlier allocator measurements. Those historical numbers predate the compact-view and bulk-operation changes. Production gameplay adoption should follow populated-workload measurements, not empty reserve alone.
