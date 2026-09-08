# Single-allocation generated SoA experiment

This DeveloperTool module contains an opt-in storage experiment. Production fighter/entity code does not depend on it. `EntityData` and `SingleAllocationEntityData` are generated from the same experimental schema and share their exact `View` and `ConstView` types.

## Generator integration

The C++ generator's ordinary SoA lowering emits views and a `TArray`-backed owner. The optional schema field

```json
"experimental_single_allocation": {"name": "SingleAllocationEntityData"}
```

adds a sibling owner and its generated `<OwnerName>Storage` base. The base owns allocation state, layout constants, and typed column operations; the owner constructs the unchanged shared views. `StorageOperations` uses C++23 explicit-object parameters for common size and mutation control flow. Its minimum mutation API is always generated, independently of the ordinary owner's `operations` list. Owner-specific handwritten functions are not copied to the sibling. Shared view functions remain available.

Nested members must identify a generated schema in the same module:

```json
{"name": "locations", "kind": "nested", "type": "Vectors", "nested_schema": "Vectors"}
```

Declare children before parents. Flattening follows depth-first member declaration order, using the same traversal as fixed SoA storage. The existing `fixed_schema` configuration and fixed output are preserved. Opaque handwritten nested owners are rejected; nested ownership is never embedded in the experimental block.

The comparison schema mirrors the fighter schema's 32 top-level members and 53 leaves, including ten vector triples. Handle and enum equivalents preserve the original sizes. Countdown schemas preserve their leaf widths and nesting, but do not implement production countdown behavior. Production countdown views reference their owners, and ordinary tick countdowns also carry tick/cleaner state; this experiment does not claim compatibility with those concrete types or benchmark their gameplay logic.

## Layout and alignment

Capacity is always a multiple of 64, including zero. For leaf `T_i`, its block describes **64 contiguous elements**:

```text
A_i = max(64, alignof(T_i))
O_i = align_up(previous_block_end, A_i)
E_i = O_i + 64 * sizeof(T_i)
A   = max(all A_i)
B   = align_up(last E_i, A)
```

The allocation has alignment `A` and size `(capacity / 64) * B`. Leaf `i` starts at `data + (capacity / 64) * O_i` and holds `capacity` contiguous elements. This is column-major SoA, not an interleaving of 64-row chunks.

Every `O_i` is divisible by its alignment, so multiplying it by any integer block count preserves that alignment. Each next offset is at least the previous end; scaling both by the same block count preserves non-overlap and enough space for every column. Base alignment satisfies every leaf alignment. All alignment and offset information is compile-time; only scaling by capacity happens at runtime.

For ordinary types with alignment at most 64, every 64-element region is already a multiple of 64 bytes, so no inter-column padding is needed. Over-aligned types can require padding, including when a small leaf precedes them. Padding in this formulation scales with the number of capacity blocks. It is reported separately from row slack. There is no packing optimizer and no promise of minimal allocation size.

Compile-time checks reject invalid alignment/overflow and alignments that cannot fit the Unreal allocator argument. Runtime arithmetic checks limit capacity by both `int32` and addressable allocation size. Tests include odd-sized leaves and 32-, 64-, and 256-byte alignments.

## Lifetime and operations

The owner contains one pointer, one `int32` count, and one `int32` capacity. V1 is explicitly non-copyable. Moves transfer ownership and leave an empty reusable source; destruction frees the allocation.

Leaves must be complete, non-cv, non-array object types satisfying all of:

- `std::is_trivially_copyable_v<T>`
- `std::is_trivially_copy_constructible_v<T>`
- `std::is_trivially_destructible_v<T>`
- `std::is_nothrow_default_constructible_v<T>`

The extra trivial-copy-construction restriction provides a conservative implicit-lifetime subset. Trivial destruction alone is insufficient. Unreal's same-type relocation helper is deliberately not used as the capability test because it accepts more than this subset. Location-dependent invariants, such as self-pointers requiring fixup after relocation, are outside the contract.

`FMemory::Malloc` obtains aligned storage. Non-allocating placement construction of an uninitialized `std::byte` array establishes the storage-providing array and permits implicit creation of the supported leaf arrays. It performs no extra allocation or initialization loop. Bulk pointer builders use `std::launder`. Mutable and const pointers share one aggregate definition. Checked builders handle null storage once; private unchecked builders receive the block count and are used only with valid allocations. Separate offset and no-offset overloads avoid unnecessary offset arithmetic. Pointer aggregates are temporary and never cached in the owner; unallocated owners return null pointers without pointer arithmetic. The lifetime basis is the C++ object model's rules for starting byte-array lifetimes and implicit object creation, not an assumption that `reinterpret_cast` constructs objects. See [C++ object model](https://eel.is/c++draft/intro.object) and [new expressions](https://eel.is/c++draft/expr.new).

`add_uninitialised` checks the new row count once, grows if necessary, and updates the authoritative count. It does not initialize values; callers must write before reading them. `add_defaulted` and growing `set_num` use Unreal's `DefaultConstructItems<T>`, matching `TArray` zero-construction traits and default constructors. A trivially copyable handle with non-zero default member initializers is supported and tested.

`reserve` rounds the requested capacity without geometric slack. Append growth reuses `DefaultCalculateSlackGrow` with allocator byte quantization disabled, clamps to the maximum supported capacity, and rounds to 64. Growth allocates one new block, copies the active bytes of each leaf, frees the old block, and commits the new capacity. It temporarily owns both allocations and cannot exploit an in-place `Realloc` as individual `TArray`s sometimes can.

`reset`, resize-down, and `remove_at_swap` retain capacity. `EAllowShrinking` parameters are accepted for call compatibility and never trigger shrinking. Swap removal follows `TArray`'s range-removal row ordering. Deep copying, append/copy from views, and non-trivial relocation/destruction are deferred.

Views use the existing `get_view`/`get_const_view` names, including ranges and slice helpers. Column addresses are calculated when views are constructed. Growing capacity invalidates old views. The owner has no per-column size invariant; ordinary view validation remains available because views can still be constructed independently.

## Validation and measurement

Generator coverage uses existing GTest structural tests, the generated C++ compile fixture with the real experimental storage helper and Unreal API stubs, and expected compilation failures for unsupported copy/destructor/default-constructor types. Catch2 runtime tests use the real Unreal allocator and generated module. They cover layout, boundaries, every leaf across repeated growth, mutation, default construction, moves, views, and arithmetic limits.

Build and run only this experiment from the repository root:

```powershell
cmake --workflow --preset single-allocation-soa-benchmark
```

The workflow configures and builds the Development benchmark target, then runs correctness, allocation diagnostics, and the timed comparison serially. CSV result rows are printed to the console, including successful runs. A final build step automatically generates PNG/SVG plots and extracted CSVs under `out/build/benchmark/single-allocation-soa-plots/` using `uv` and matplotlib. It uses the default 4,096, 65,536, and 1,048,576 entity counts. Stop competing workloads before running it. The latest test log is under `out/build/benchmark/Testing/Temporary/LastTest.log`.

To rerun without rebuilding:

```powershell
ctest --preset single-allocation-soa-benchmark
```

### Plotting results

The workflow generates plots automatically. To regenerate its plots without rerunning the benchmark:

```powershell
cmake --build --preset single-allocation-soa-plots
```

You can also invoke the matplotlib script directly:

```powershell
uv run Scripts/plot-single-allocation-soa-benchmarks.py
```

`uv` installs the script's declared matplotlib dependency in an isolated environment. Python 3.11+ is required. If matplotlib is already installed, `python Scripts/plot-single-allocation-soa-benchmarks.py` also works. Plotting is headless and does not run the benchmark again. Images are written through a temporary sibling file and replaced atomically. If an image viewer prevents replacement, a uniquely named image is retained and its path is printed; close the old image before rerunning to restore the usual filename.

By default the script reads `out/build/benchmark/Testing/Temporary/LastTest.log` and writes under `.local/benchmarks/single-allocation/plots/`:

- `timings.png` / `.svg`: per-operation median durations in milliseconds on logarithmic axes, with p10–p90 error bars.
- `relative-performance.png` / `.svg`: TArray median divided by single-allocation median. Above 1 means single allocation is faster; below 1 means slower. Colour is symmetric in log space around equal performance.
- `allocations.png` / `.svg`: requested/usable storage, row slack, peak requested-byte bounds, capacity-changing requests, and retained allocations, split by row count and reserve mode.
- `timings.csv` and `allocations.csv`: extracted source records for later inspection or replotting.

CTest replaces its latest log on subsequent test runs. Save it before running other tests if you want to keep a measurement. A log containing only allocation diagnostics cannot produce timing plots. You can pass saved logs, captured verbose CTest console output, or the exported CSV files explicitly:

```powershell
uv run Scripts/plot-single-allocation-soa-benchmarks.py --input .local/benchmarks/single-allocation/timed-comparison.log .local/benchmarks/single-allocation/allocation-diagnostics.log --output-dir .local/benchmarks/single-allocation/plots
```

Use files from one measurement run. Conflicting duplicate results are rejected rather than silently combined. Timing-only inputs generate the two timing figures; allocation figures require allocation records. Missing operation/count combinations are left blank. Error bars show sample variability, not confidence intervals. Reserved append/resize durations exclude reserve cost, and uninitialised append does not write row contents. The allocation peak chart is a bound, not measured RSS.

Relevant commands, run from the repository root:

```powershell
cmake --workflow --preset codegen
cmake --build --preset generate-code
cmake --workflow --preset format-code
cmake --workflow --preset generate-project-files
cmake --workflow --preset debug-game
ctest --preset debug-game-unit-tests -L low-level-core --output-on-failure
cmake --preset benchmark
cmake --build --preset benchmark
ctest --preset benchmark-tests -R 'SingleAllocation.BenchmarkCorrectness' --output-on-failure
ctest --preset benchmark-tests -R 'SingleAllocation.AllocationDiagnostics' -V
```

After builds/setup finish and the user confirms that competing work has stopped:

```powershell
ctest --preset benchmark-tests -R 'SingleAllocation.TimedComparison' -V
```

The benchmark uses Development optimization with matching AVX2/precise floating-point settings. Default sizes are 4,096, 65,536, and 1,048,576 rows. Each scenario gets three untimed warm-ups and 31 paired samples, alternating owner order. Output includes medians and 10th/90th sample percentiles; these percentiles describe variability, not confidence intervals. Allocation/setup and destruction are outside the timed intervals. Each run has bounded fixture memory. Results are observable and verified outside timing.

Append cases use the same non-inlined call boundary for both owners, with 1- and 64-row batches. Natural and reserve-first uninitialized append are measured separately; defaulted append uses pre-reserved capacity. Isolated reserve measures allocator cost without touching every page. Populated growth forces both layouts to grow and preserve their initialized data. Normal-operation cases include defaulted `set_num`, swap removal, and amortized reset/resize-down reuse. The latter measure 4,096 **reset-plus-uninitialized-refill** or **shrink-one-plus-uninitialized-append** cycles; they are not reported as isolated reset/shrink costs. Divide their durations by 4,096 for cycle cost. Append throughput is `count / seconds`; removal throughput is `(count / 4) / seconds`.

Iteration calls the same non-templated function through identical view types, with view construction outside timing. It performs four passes; a wider case touches additional columns. View construction plus a shared non-inlined consumer is also measured across 4,096 calls. Generated ordinary owner mutations are out-of-line as in the current generator; the experimental fast path is inline. This is a comparison of the generated implementations, not an attempt to remove their API implementation costs.

Untimed allocation diagnostics track capacity changes as allocation requests, not physical heap allocations. They report retained blocks, requested/allocator-usable bytes, live bytes, row slack, layout padding, min/max column capacity, and owner size. Peak requested bytes are a conservative bound assuming each old leaf allocation remains live while its replacement is allocated. That bound is exact for the experimental owner; the baseline may use in-place reallocation. Allocator metadata/fragmentation and system RSS are not measured.

The initial confirmed run showed faster reserved append/reuse but substantial reserve and growth regressions; keep this experimental. Reserve times include allocator behavior and should not be interpreted as layout-math cost. Inspection of the optimized MSVC benchmark object confirms that the byte-array placement new emits no instructions: empty reserve calls `FMemory::Malloc` and skips all column-pointer/copy work. The allocator receives one large 64-byte-aligned request, while the ordinary owner uses separate per-column reallocations. Allocation diagnostics print `SOA_ALLOCATOR` to identify the active allocator. The diagnostic run reports Mimalloc. This Unreal Windows build selects mimalloc 2.0.0, whose large-object limit is 4 MiB (`Engine/Source/ThirdParty/mimalloc/2.0.0/include/mimalloc-types.h`). At 65,536 rows the single block is 12.1875 MiB, versus at most 512 KiB per ordinary column. It crosses into mimalloc's dedicated huge-segment allocation path while the individual columns do not. Huge-segment freeing is forced rather than normal small/large-page reuse (`src/segment.c`). Here “huge” is mimalloc's object-size classification, not added OS large-page support. This explains why allocation count alone cannot predict reserve cost; attributing exact time to caching, commit, or page faults still requires profiling.

The placement new is retained to establish the storage-providing byte array and implicit-lifetime objects without relying on the implementation of Unreal's custom allocator. C++ does not require this particular spelling: standard allocation functions and C++23 lifetime-start facilities offer other routes. Removing it solely for speed is unsupported by the emitted code.

Generated modules opting into single-allocation storage run through clang-format before writing or checking outputs, using the nearest project `.clang-format`. Formatting failures leave existing outputs untouched. Other generator modes retain their existing formatting policy.
