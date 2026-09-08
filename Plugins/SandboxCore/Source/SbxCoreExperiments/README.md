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

### Selecting individual operations for profiling

Each timing operation is a separate Catch2/CTest test under `SandboxCore.SingleAllocation.Timing.<operation>`. Each test compares both owners across the configured row counts, using Catch2 BENCHMARK_ADVANCED calibration and statistical analysis. Only the selected operation runs. Timing boundaries are described below.

Reserve only, including build and timing plots:

```powershell
cmake --workflow --preset single-allocation-soa-reserve
```

Rerun reserve without building or plotting:

```powershell
ctest --preset single-allocation-soa-reserve
```

Select another operation, or combine operations with a regex:

```powershell
ctest --preset single-allocation-soa-benchmark -R '^SandboxCore\.SingleAllocation\.Timing\.populated_growth$'
ctest --preset single-allocation-soa-benchmark -R '^SandboxCore\.SingleAllocation\.Timing\.(reserve|populated_growth)$'
ctest --preset single-allocation-soa-benchmark --show-only
```

Operation suffixes are `natural_append_1`, `natural_append_64`, `reserved_append_1`, `reserved_append_64`, `defaulted_append_1`, `defaulted_append_64`, `reserve`, `populated_growth`, `set_num_grow`, `set_num_shrink_reuse`, `reset_reuse`, `remove_swap`, `iterate`, `iterate_wide`, and `construct_views`.

The full workflow still runs all timing operations plus correctness and allocation diagnostics. Each timing operation now runs in its own process, giving it a fresh allocator history instead of inheriting earlier operations' allocations. This can change timings relative to the original monolithic run. The Catch2 CSV format replaces the historical manual-timer format. Partial runs produce plots for the selected operations only; they do not contain allocation diagnostics unless those were selected too.

### Plotting results

To extract and plot reserve timings from saved results without manually filtering CSV:

```powershell
uv run Scripts/plot-single-allocation-soa-benchmarks.py --input .local/benchmarks/single-allocation/timings.csv --operation reserve --output-dir .local/benchmarks/single-allocation/reserve-plots
```

Repeat `--operation` to select multiple timing operations. The exported `timings.csv` contains only the selected operations. Allocation diagnostics, when supplied, remain unfiltered because they describe whole containers rather than timing operations.

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

- `timings.png` / `.svg`: per-operation Catch2 mean durations in milliseconds on logarithmic axes, with confidence bounds.
- `relative-performance.png` / `.svg`: TArray mean divided by single-allocation mean. Above 1 means single allocation is faster; below 1 means slower. Colour is symmetric in log space around equal performance.
- `allocations.png` / `.svg`: requested/usable storage, row slack, peak requested-byte bounds, capacity-changing requests, and retained allocations, split by row count and reserve mode.
- `timings.csv` and `allocations.csv`: extracted source records for later inspection or replotting.

CTest replaces its latest log on subsequent test runs. Save it before running other tests if you want to keep a measurement. A log containing only allocation diagnostics cannot produce timing plots. You can pass saved logs, captured verbose CTest console output, or the exported CSV files explicitly:

```powershell
uv run Scripts/plot-single-allocation-soa-benchmarks.py --input .local/benchmarks/single-allocation/timed-comparison.log .local/benchmarks/single-allocation/allocation-diagnostics.log --output-dir .local/benchmarks/single-allocation/plots
```

Use files from one measurement run. Conflicting duplicate results are rejected rather than silently combined. Timing-only inputs generate the two timing figures; allocation figures require allocation records. Missing operation/count combinations are left blank. Error bars show Catch2 confidence intervals for the mean (95% by default). Reserved append/resize durations exclude reserve cost but include resetting the logical size; uninitialised append does not write row contents. The allocation peak chart is a bound, not measured RSS.

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
ctest --preset benchmark-tests -R '^SandboxCore\.SingleAllocation\.Timing\.' -V
```

The benchmark uses Development optimization with matching AVX2/precise floating-point settings. Default sizes are 4,096, 65,536, and 1,048,576 rows. Catch2 controls calibration, warmup, repetitions, samples, and bootstrap analysis. Owners run sequentially (TArray then Single), rather than alternating manual samples. Correctness is checked separately by BenchmarkCorrectness. A listener exports SOA_CATCH records containing count, operation, owner, mean/lower/upper nanoseconds, confidence level, iterations per sample, and sample count. The existing CMake plotting step consumes these records automatically. Historical manual logs are rejected because their measurement boundaries and statistics differ.

Append cases use the same non-inlined call boundary for both owners, with 1- and 64-row batches. Natural and reserve-first uninitialized append are measured separately; defaulted append uses pre-reserved capacity. Reserve now measures construction + reserve + destruction, without explicitly touching every page. Natural append also includes destruction. Populated growth measures the complete prepare + grow + destroy lifecycle, including initial allocation, defaulting and initialization; it is no longer an isolated growth measurement. This keeps memory bounded: preparing a separate 195 MiB owner for every calibrated repetition could exhaust memory. Reserved append/defaulting/set_num reuse one prepared owner and include reset before each repetition. Swap removal restores the logical row count inside timing before removing rows; row values are not reinitialized between repetitions. Normal-operation cases include defaulted `set_num`, swap removal, and amortized reset/resize-down reuse. The latter measure 4,096 **reset-plus-uninitialized-refill** or **shrink-one-plus-uninitialized-append** cycles; they are not reported as isolated reset/shrink costs. Divide their durations by 4,096 for cycle cost. Append throughput is `count / seconds`; removal throughput is `(count / 4) / seconds`.

Iteration reuses the same initialized owner across repetitions, so values accumulate. It calls the same non-templated function through identical view types, with view construction outside timing. It performs four passes; a wider case touches additional columns. View construction plus a shared non-inlined consumer is also measured across 4,096 calls. Generated ordinary owner mutations are out-of-line as in the current generator; the experimental fast path is inline. This is a comparison of the generated implementations, not an attempt to remove their API implementation costs.

Untimed allocation diagnostics track capacity changes as allocation requests, not physical heap allocations. They report retained blocks, requested/allocator-usable bytes, live bytes, row slack, layout padding, min/max column capacity, and owner size. Peak requested bytes are a conservative bound assuming each old leaf allocation remains live while its replacement is allocated. That bound is exact for the experimental owner; the baseline may use in-place reallocation. Allocator metadata/fragmentation and system RSS are not measured.

The initial Unreal run showed faster reserved append/reuse but substantial reserve and growth regressions; keep this experimental. Inspection of the optimized MSVC benchmark object confirms that the byte-array placement new emits no instructions. However, the earlier attribution to mimalloc's huge-allocation path overlooked Unreal's poisoning proxy. In `Engine/Source/Runtime/Core/Public/HAL/MallocPoisonProxy.h`, `Malloc` fills every requested byte with `0xcd`, whereas `Realloc(nullptr, size)` leaves `OldSize` at zero and skips the new-memory fill. TArray's allocator uses Realloc while the experimental owner uses Malloc. Consequently an empty reserve through this proxy performs substantially different work: the single owner touches its entire block. `Free` also poisons allocations, affecting lifecycle timings.

`UE_USE_MALLOC_FILL_BYTES` normally enables the proxy in non-editor Debug/Development builds without ASan or a fixed GMalloc class. The benchmark's generated definitions confirm Development with `WITH_EDITORONLY_DATA=0`; the wrapper is installed conditionally in `Core/Private/HAL/UnrealMemory.cpp`. The diagnostic allocator name alone cannot establish whether poisoning is enabled because the proxy forwards `GetDescriptiveName()` to its underlying allocator. The code establishes the Malloc/Realloc asymmetry; quantifying its contribution still requires a controlled Unreal comparison with poisoning held consistent. The mimalloc object-size threshold may also matter, but the previous measurements did not isolate it from poisoning.

The placement new is retained to establish the storage-providing byte array and implicit-lifetime objects without relying on the implementation of Unreal's custom allocator. C++ does not require this particular spelling: standard allocation functions and C++23 lifetime-start facilities offer other routes. Removing it solely for speed is unsupported by the emitted code.

Generated modules opting into single-allocation storage run through clang-format before writing or checking outputs, using the nearest project `.clang-format`. Formatting failures leave existing outputs untouched. Other generator modes retain their existing formatting policy.

## Standalone standard-library experiment

This is an additional backend and benchmark executable. The Unreal types, Catch2 benchmarks, and workflows remain available.

Build and validate without running a performance comparison:

```powershell
cmake --workflow --preset native-soa
```

This builds the generator, generated native owners, Google Benchmark executable, and runtime tests. It runs correctness tests, one-iteration benchmark dry runs, and a JSON-to-plot smoke check. Dry-run timings are validation artifacts, not performance results.

Run the reserve comparison and generate plots after stopping competing workloads:

```powershell
cmake --workflow --preset native-soa-reserve
```

This compares both owners at 4,096, 65,536, and 1,048,576 rows, with nine randomly interleaved repetitions and a 0.1-second minimum measured duration per repetition. Results are saved to `out/build/native-soa/native-soa-results.json`; plots go to `out/build/native-soa/native-soa-plots/`. Google Benchmark JSON retains individual repetitions, aggregates, units, configuration context, and memory counters. The PNG plot shows means and min–max ranges across repetitions, not confidence intervals.

The standalone executable is `out/build/native-soa/Codegen/native_soa/native-soa-benchmarks.exe`. It includes optimized debug symbols for profiling. To select cases directly in a profiler, use arguments such as:

```text
--benchmark_filter=Vector/reserve/65536/ --benchmark_min_time=5s
--benchmark_filter=Single/reserve/65536/ --benchmark_min_time=5s
```

Use `--benchmark_list_tests=true` to list cases. Other operation names include `populated_growth`, `defaulted_append_1`, `defaulted_append_64`, `reserved_defaulted_append_1`, `reserved_defaulted_append_64`, `set_num`, `reset_refill_16`, `remove_swap`, `iterate`, and `views_4096`. `reserved_uninitialised_append_1` exists only for Single; standard vector has no equivalent operation. Ordinary defaulted append uses vector resize and the single owner uses value construction, including Handle's default -1 values.

The generator accepts `experimental_stdlib: true` on an SoA module. It emits header-only vector owners and nested mutable/const span views. The native experiment's small generation driver reads the canonical Unreal experimental schema, projects integer names and leaf-type dependencies into standard C++, and selects this backend. Field order and nesting therefore come from the same source manifest. Generated output is under `out/build/native-soa/Codegen/native_soa/generated/native_soa_types.h`; edit the manifest or generator rather than this build artifact.

Both backends use the existing flattened-layout generator and a shared standard-C++ layout/trait header. Capacity granularity remains 64; every column starts on at least a 64-byte boundary, raised for over-aligned leaves, and the base allocation satisfies the maximum alignment. Native runtime tests include alignments 32, 64, and 256, boundary capacities, non-overlap, data preservation, moves, default values, nested spans, removal, and checked arithmetic. The single owner retains the same trivial-copy/destruction and nothrow-default-construction restrictions and remains move-only. Native single growth uses 1.5x geometric growth rounded to 64; vector uses the standard-library implementation's growth policy.

The native backend supports reserve, reset, set_num, add_defaulted, remove_at_swap, and shared get_view APIs; the single owner also supports uninitialised append and sliced views. Fixed storage, equivalent types, custom generated functions, and vector<bool> are outside this backend's current scope. Native views share an API with each other, not a C++ type with Unreal TArrayViews. Vector owners use ordinary vector copy/move behavior; mutations across multiple columns are not transactionally rolled back after allocation failure. This prototype does not claim full parity with the ordinary Unreal generator.

Allocation uses standard aligned operator new/delete for Single and normal std::vector allocation for Vector. The active native compiler/STL and allocator differ from the Unreal configuration, so native timings must not be treated as measurements of FMemory/Mimalloc. The current preset uses Release optimization, precise floating-point operations, AVX2, and debug information on the benchmark target.

Reserve and populated growth exclude fixture setup and final destruction using Google Benchmark pause/resume timing; append excludes resetting or destroying the previous fixture. Growth starts with initialized live rows and requests 2 * count + 1 capacity. Memory stays bounded to one fixture per executing case. Timing transitions occur once per whole container operation, not per row; very short operations can still be affected by framework overhead. Iteration uses identical span-based source code on initialized owners, and values accumulate between repetitions. Swap removal restores the row count outside timing but does not restore removed values. Reset/refill measures 16 defaulted refill cycles; views measures 4,096 constructions. These boundaries differ from the Catch2 lifecycle benchmarks.

JSON counters report retained capacity bytes, retained allocations, live bytes, unused bytes (including layout padding), and owner size. They do not count heap calls, allocator metadata, transient growth peaks, or resident memory. Throughput counts rows for append/iteration, removed rows for removal, requested rows for reserve, refilled rows for reset/refill, and view constructions for views.

To replot saved JSON:

```powershell
uv run Scripts/plot-native-soa-benchmarks.py --input out/build/native-soa/native-soa-results.json --output-dir out/build/native-soa/native-soa-plots
```

The confirmed native reserve run on 2026-09-08 used nine randomly interleaved repetitions. Mean wall times were:

| Rows reserved | Vector | Single | Vector / Single |
|---:|---:|---:|---:|
| 4,096 | 8.727 us | 0.242 us | 36.0x |
| 65,536 | 47.945 us | 2.976 us | 16.1x |
| 1,048,576 | 149.159 us | 3.330 us | 44.8x |

Requested storage matched at every count; retained allocations were 53 versus 1. These are empty-reserve measurements with cleanup excluded and no explicit row initialization. They establish that the large Unreal reserve regression did not reproduce in this native configuration, not that allocation cost is independent of allocator configuration or page touching. Local JSON and PNG artifacts are under the paths above and are not committed.