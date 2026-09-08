# Generated single-allocation SoA

The `single_allocation` schema setting adds an opt-in owning representation alongside the normal TArray-backed generated owner:

```json
"single_allocation": {"name": "SingleEntityData"}
```

The runtime is part of SandboxCore. Comparison types remain in SbxCoreExperiments; production fighter storage has not been migrated. The former `experimental_single_allocation` setting has been renamed; repository manifests have been updated rather than maintaining two spellings.

## Ownership and layout

An owner stores one allocation pointer, one `int32` size and one `int32` capacity: 16 bytes on Win64. Each flattened leaf occupies a contiguous column in the allocation. Nested schemas flatten in depth-first declaration order using the fixed-SoA traversal.

Capacity is a multiple of 64. Compile-time offsets describe a 64-element block for each column; actual column offsets scale by `capacity / 64`. This remains column-major storage, not interleaved blocks of rows. The allocation and each column are aligned to at least 64 bytes, raised for over-aligned leaves. Padding is allowed and scales with capacity blocks. Existing checks and tests cover alignment through 256 bytes.

Reserve rounds to the granularity. Append growth uses geometric slack before rounding. Growth allocates one new block, bulk-copies each live column, then releases the old block. Allocation-size and row-count arithmetic are checked. Owners remain move-only; reset and removal retain capacity.

Leaves must be non-cv, non-array object types that are trivially copyable, trivially copy constructible, trivially destructible and nothrow default constructible. Location-dependent invariants requiring relocation fixups are unsupported. Byte-array placement construction establishes the supported implicit lifetimes without initializing the allocation. Defaulted additions use the backend's normal default/value construction policy.

## Compact views

`View` and `ConstView` each store an owner-state pointer, row offset and row count. Both are 16 bytes and trivially copyable; generated nested views have the same guarantees. Allocator variants share view types for the same schema.

```cpp
auto rows = data.get_view();
auto healths = rows.healths();
auto xs = rows.locations().xs();
auto selected = rows.slice(32, 64);
auto readonly = selected.get_const_view();
```

Views capture a range, not a growing count. They follow their owner across reserve/growth by resolving columns on access. Moving or destroying the owner invalidates views. Shrinking/removal can invalidate a range or change which entities it denotes; views are not stable entity references. Access checks validate the range against the current owner size.

Returned column spans are ordinary borrowed pointers: growth invalidates them. Acquire spans outside hot loops and reacquire after growth. Mutable views remain writable when the view object itself is const, like an ordinary span; `ConstView` exposes const elements.

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

Unreal single owners default to `ml::soa_storage::MimallocStorageAllocator`. SandboxCore stages vcpkg mimalloc as `sbx-mimalloc.dll`, loads its exports explicitly and retains the DLL for process lifetime. Allocation, reallocation and free use that DLL; Unreal's global allocator is unchanged. Explicit loading avoids collisions with Unreal's own mimalloc symbols. This integration currently requires Win64 x64 and the dynamic CRT, as enforced by the module rules.

Allocator variants select a type providing `allocate(bytes, alignment)` and `free(data)`. The FMemory comparison remains explicit. The native backend retains its standard/mimalloc configuration choices and uses the same layout and compact-view machinery without linking Unreal.

## Validation and measurement

The test suites cover compact view sizes, slicing and growth-following behavior, aligned bulk copies, self-append, empty inputs, overflow, descending removal subsets and invalid indices. Use the repository CMake workflows for codegen, native tests and Unreal core tests.

The [comparison README](../SbxCoreExperiments/README.md) documents benchmark commands; the [investigation report](../SbxCoreExperiments/INVESTIGATION.md) preserves earlier allocator measurements. Those historical numbers predate the compact-view and bulk-operation changes. Production gameplay adoption should follow populated-workload measurements, not empty reserve alone.
