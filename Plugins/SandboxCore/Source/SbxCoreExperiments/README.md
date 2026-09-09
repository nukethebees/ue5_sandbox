# Single-allocation generated SoA experiment

The comparison uses an opt-in generated owner with one aligned allocation, one size and one capacity for all 53 flattened columns. The runtime and generator mode are now production facilities in SandboxCore, with mimalloc as the Unreal default. Comparison schemas remain experimental and production fighter data is unchanged. See the [storage API and lifetime contract](../SandboxCore/single_allocation_storage.md).

The generated single-allocation default now includes a fixed **192-byte gap**
between columns, followed by any padding required for the next column's alignment.
This applies to Unreal and native owners and all allocator variants. The
[fixed-CPU report](#fixed-cpu-results-9-september-2026) below records the evidence
used to select it; those archived timings precede the generator change.

The [investigation report](INVESTIGATION.md) records the layout/lifetime design, generator architecture, allocator investigation, measured results, limitations, and instructions for recreating the full experiment. Historical CSVs and PNG plots are preserved in [results](results/).

## Quick comparison

Stop competing workloads, then run from the repository root:

```powershell
cmake --workflow --preset single-allocation-soa-benchmark
```

This builds the Development benchmark executable, runs correctness, and compares these two representations:

| Test suffix | Generated owner | Backing allocation |
|---|---|---|
| `reserve_TArray` | `EntityData` | 53 ordinary TArrays / Unreal allocator |
| `reserve_SingleMimalloc` | `SingleAllocationEntityData` | One direct mimalloc allocation |

Each reserve invocation constructs **one owner**, reserves **65,536 rows**, and destroys it. No rows are explicitly initialized. Catch2 calibrates repetitions and collects its default 100 samples; this is one reserve per invocation, not one timing sample. CTest launches each reserve implementation in a separate process and runs serially. There is no owner sweep or raw allocator matrix. This comparison measures both representation and allocator differences; it is not the same-allocator experiment in the report.

The full workflow also measures natural/reserved/defaulted append (batches of 1 and 64), populated growth, set-num grow/shrink, reset/reuse, swap removal, narrow/wide iteration and view construction. All use 65,536 rows and only TArray versus Single/Mimalloc. These operations remain separate tests for profiling; each compares both owners. Iteration reuses initialized storage, and reserved operations exclude initial reserve. See the report for the complete measurement boundaries.

Actual-value append is measured separately by `append_from_natural_64`, `append_from_natural_65536`, `append_from_reserved_64` and `append_from_reserved_65536`. Source construction/defaulting is outside timing. Natural cases include destination allocation and destruction; reserved cases reuse storage and include reset. Sources contain 65,536 rows, copied in 64-row batches or one whole-source operation.

`remove_indices_clustered` and `remove_indices_scattered` remove 16,384 original rows from a 65,536-row owner. Index-list construction is untimed; timing includes restoring logical size/defaulting before removal, but does not restore original row values. Both sides use ascending surviving-tail block order. TArray has no equivalent generated index-list operation, so its benchmark adapter uses the same movement-range traversal with column copies and a final per-array resize. This avoids comparing different survivor ordering.

`view_handles` measures compact handle creation/consumption, while `materialize_columns` measures conversion to the existing column-span aggregate. Each performs 4,096 calls; `construct_views` retains the existing aggregate-consuming workload. Iteration materializes spans outside the timed loop for both owners. Compact views are 16 bytes each, but the explicitly materialized aggregate is still large. These cases are also registered in the optional native Google Benchmark executable, whose vector append validates source column lengths and rejects aliasing; self-append support belongs to the single owner.

Correctness includes `BulkBenchmarkCorrectness` before timing. Historical measurements in the report predate these APIs and should not be interpreted as results for the current implementation.

Plots are generated automatically by the final CMake step:

- `out/build/benchmark/single-allocation-soa-plots/timings.png` and `relative-performance.png` (other operations)
- `out/build/benchmark/single-allocation-soa-plots/reserve-comparison-65536.png`
- `out/build/benchmark/single-allocation-soa-plots/reserve-records.csv` (nanoseconds, confidence bounds, samples)

The bar chart uses a logarithmic time axis so both means remain visible. SVG is also exported, but PNG is the convenient default. CTest's source log is `out/build/benchmark/Testing/Temporary/LastTest.log`; save it before another test run replaces it. Existing plots from previous runs are not deleted.

Reserve without correctness, including build and plotting:

```powershell
cmake --workflow --preset single-allocation-soa-reserve
```

Rerun with a custom sample count after building (two processes, saved logs and plots):

```powershell
./Scripts/run-soa-reserve-benchmark.ps1 -Samples 100
```

The script writes to `.local/benchmarks/single-allocation/reserve/`; use `-OutputDirectory` to preserve another run. For profiling, select either exact Catch2 test name, such as `SandboxCore.SingleAllocation.Timing.reserve_TArray`. To rerun via CTest or regenerate plots:

```powershell
ctest --preset single-allocation-soa-reserve
cmake --build --preset single-allocation-soa-plots
```

The plotting script uses `uv` and its declared matplotlib dependency. It also accepts saved logs with `--input` and `--output-dir`. Images are replaced atomically; if a viewer locks an image, a new filename is printed instead of losing the result.

## Correctness and optional native experiment

```powershell
cmake --preset benchmark
cmake --build --preset benchmark
ctest --preset benchmark-tests -R 'SingleAllocation.BenchmarkCorrectness' --output-on-failure
cmake --workflow --preset native-soa
```

The native workflow runs correctness and dry-run/plot validation, not useful performance measurements. The more expensive `native-soa-reserve` and `native-soa-reserve-matrix` workflows remain available only when explicitly selected. See the report before comparing their results: the original native reserve benchmark excludes destruction, whereas the matrix and Unreal comparison include it.
## Column-spacing investigation

The opt-in `single-allocation-soa-spacing` workflow builds, checks and runs the
spacing experiment, then generates PNG plots. It does not change production
storage or run as part of the usual single-allocation comparison.

```powershell
# Complete preparation before starting a timed run:
cmake --preset benchmark
cmake --build --preset benchmark
ctest --preset single-allocation-soa-spacing-correctness

# After stopping competing work, run the staged sweep and plotting:
cmake --build --preset single-allocation-soa-spacing

# Alternatively, perform the complete workflow:
cmake --workflow --preset single-allocation-soa-spacing
```

The runner is `Scripts/run-soa-spacing-experiment.py`. It supports `--dry-run`,
`--seed`, `--output-dir`, and `--report-only --output-dir <saved-run>`.
Each default run creates a timestamped directory under
`.local/benchmarks/single-allocation/spacing`. Existing runs are never overwritten.
The correctness preset also exercises the full runner/export/plot pipeline with
one unanalysed sample per case; these smoke measurements are not performance evidence.

The original generated layout used `base + (capacity / 64) * column_block_offset`, with
compile-time offsets for 64 elements. The benchmark-only owner reproduces those
offsets at zero gap and adds alignment-preserving gaps between columns. It uses
the same Mimalloc wrapper and generated aggregate view. Both owners call the same
non-inlined iteration function. Allocation, initialization, diagnostics and view
construction are outside timing; each invocation performs four passes.

The main sweep holds 65,535 live rows constant. Reserve requests of 65,535,
65,536, 65,537, 75,000, 100,000 and 131,072 map to five distinct capacities:
65,536, 65,600, 75,008, 100,032 and 131,072. Gaps are 0, 64, 128, 192, 256,
384, 512, 768 and 1,024 bytes. Smaller nonzero gaps would round to 64 bytes.
Generated-owner controls run at every capacity; a TArray control and the original
65,536-live-row comparison are included. Every case/kernel gets a fresh process.
Process order is shuffled with a recorded seed; processes run sequentially.

The initial 108 cases use 30 Catch2 samples. For each capacity, the zero-gap,
fastest-wide and slowest-wide layouts are selected and deduplicated. Both kernels
then run in two additional fresh processes with 100 samples, alongside generated
and TArray controls. Expect several minutes of runtime. The sweep changes no
allocator settings, thread affinity or system configuration.

Outputs include per-process logs, `results.json`, `timings.csv`, `columns.csv`,
initial heatmaps/curves and confirmation plots with individual confidence bounds.
Metadata records source revision/diff, CPU, binary hashes, dependency versions,
build configuration and process order. Column ordinals follow generated
`apply_arrays` traversal; each record includes element size, alignment, base
address, allocation offset, extent, next-column stride/gap and address residues
modulo 64, 4,096 and 65,536. TArray has no shared allocation base; its addresses
and inter-column address differences are diagnostic, not contiguous padding.

When LLVM's `llvm-objdump` is available, the runner also saves the shared kernel's
disassembly and compiler response file. Inspection of the current Development
object shows scalar `vmulss`/`vaddss` operations and array-view bounds checks in
the loop. These instructions are identical across layouts; conclusions apply to
this kernel/build and do not establish the behaviour of a vectorized kernel.

Interpret repetitions separately. Virtual address residues cannot prove physical
cache-set mappings or establish a hardware cause. Recommend a fixed gap only if
repeat measurements support it across capacities: prefer the smallest gap within
10% of the best tested narrow and wide timings at every capacity, without a
repeatable regression exceeding 10% against zero gap. If the selected repetitions
do not establish that condition, gather focused follow-up evidence instead of
changing production layout. Pipeline smoke results must not be used to select a policy.

### Spacing results: 8 September 2026

**Deliberate column gaps removed the large wide-iteration slowdown at both tested
power-of-two capacities.** The effect repeated in independent processes. This
supports adding a small constant column-spacing term as the next prototype, but
does not yet establish a universally robust gap or prove a particular hardware cause.

The run completed all 184 isolated processes in approximately eight minutes:
108 initial cases and 76 confirmation cases. Host: AMD Ryzen 9 9950X3D, Windows,
Unreal Development, MSVC 14.50.35737 toolchain, Mimalloc 3.5.0. Source was
`a99402ec` plus the saved experimental working-tree changes. Thread affinity was
not fixed. The complete local archive is
`.local/benchmarks/single-allocation/spacing/20260908-231323`, including raw logs,
addresses, configuration, source snapshots and disassembly.

Repository snapshots: [timings with confidence bounds](results/spacing-2026-09-08/timings.csv),
[wide sweep](results/spacing-2026-09-08/wide.png),
[narrow sweep](results/spacing-2026-09-08/narrow.png),
[wide confirmation](results/spacing-2026-09-08/wide-confirmation.png),
[narrow confirmation](results/spacing-2026-09-08/narrow-confirmation.png).
CSV owner codes are 0 = benchmark-only spaced allocation, 1 = generated
Single/Mimalloc, 2 = generated TArray. `wide=0` selects narrow iteration;
`repetition=0` is the initial 30-sample sweep, and 1/2 are fresh 100-sample processes.

Wide iteration, initial sweep means in milliseconds; every cell processes the
same 65,535 live rows through the same four-pass kernel:

| Gap (bytes) | Capacity 65,536 | 65,600 | 75,008 | 100,032 | 131,072 |
|---:|---:|---:|---:|---:|---:|
| 0 | 7.121 | 1.325 | 1.359 | 1.327 | 6.922 |
| 64 | 1.314 | 1.303 | 1.299 | 1.298 | 1.359 |
| 128 | 1.308 | 1.301 | 1.298 | 1.338 | 1.306 |
| 192 | 1.296 | 1.296 | 1.298 | 1.301 | 1.290 |
| 256 | 1.332 | 1.335 | 1.348 | 1.336 | 1.332 |
| 384 | 1.305 | 1.331 | 1.309 | 1.307 | 1.294 |
| 512 | 1.340 | 1.329 | 1.333 | 1.330 | 1.340 |
| 768 | 1.355 | 1.347 | 1.336 | 1.325 | 1.375 |
| 1,024 | 1.386 | 1.331 | 1.381 | 1.329 | 1.366 |

The nonzero-gap timings cluster tightly compared with the zero-gap slowdown.
There is no repeating severe slowdown among the tested gaps up to 1,024 bytes.
Capacity dependence is much stronger: the unpadded layouts at 65,600, 75,008 and
100,032 were already fast. The sweep does not cover an entire address period or
all capacities, so it does not establish that larger gaps or other capacities are safe.

Narrow iteration, initial means in microseconds:

| Capacity | No gap | 64-byte gap | 192-byte gap |
|---:|---:|---:|---:|
| 65,536 | 416.0 | 386.4 | 388.3 |
| 65,600 | 402.4 | 400.4 | 410.9 |
| 75,008 | 407.6 | 399.6 | 384.0 |
| 100,032 | 427.6 | 400.0 | 411.1 |
| 131,072 | 427.6 | 438.6 | 394.1 |

Across all initial gaps, narrow timings ranged from 384.0 to 439.9 microseconds.
They show no counterpart to the fivefold wide slowdown. Small rankings should
not be treated as stable: process-to-process variation is visible in confirmation
results, and neither core placement nor hardware counters were controlled.

The 192-byte gap was selected for confirmation at both power-of-two capacities.
These are separate process means, not a pooled confidence interval:

| Capacity | No gap, repeat 1 / 2 (ms) | 192-byte gap, repeat 1 / 2 (ms) |
|---:|---:|---:|
| 65,536 | 6.936 / 6.908 | 1.297 / 1.293 |
| 131,072 | 7.035 / 7.034 | 1.298 / 1.298 |

That is approximately 5.3–5.4 times faster. Generated zero-gap controls reproduced
the same slow/fast capacity classification, although exact means varied; for
example, the 65,536-capacity generated control measured 6.249 and 7.132 ms in
confirmation. TArray measured 6.719 and 6.584 ms. Independent allocations and
process scheduling remain sources of variation.

The original 65,536-live-row reproduction also held: TArray wide 6.621 ms,
generated Single/Mimalloc 6.921 ms, and generated Single/Mimalloc reserving
65,600 slots 1.341 ms. Their narrow means were 443.7, 428.8 and 391.5 microseconds.

### Address evidence and interpretation

At capacities 65,536 and 131,072, all 53 unpadded column bases had the same
residue modulo 4,096. The adjacent float-column strides were respectively
262,144 and 524,288 bytes. A 64-byte gap changed those strides to 262,208 and
524,352 bytes and produced 53 distinct column-base residues modulo 4,096.
A 1,024-byte gap produced only four distinct residues, but still avoided the
large slowdown. The number of distinct residues alone is therefore not a timing model.

For the non-power-of-two capacities, zero-gap layouts already had 39, 14 and 39
distinct residues, respectively. This correlation, the unchanged shared kernel,
and the independent-process repetitions strengthen the address-conflict hypothesis.
They do not distinguish cache-set conflicts from alias-related processor effects
or other memory-system behaviour. No hardware counters or physical address
mappings were collected. The narrow kernel touches six float columns; the wide
kernel touches 22, which makes the difference in sensitivity worth further study.

The fixed-gap overhead is `(53 - 1) * gap`, independent of capacity: 3,328 bytes
for 64, 9,984 bytes for 192, and 53,248 bytes for 1,024. All column and base
alignments were preserved. Adding 64 capacity slots instead costs 12,480 bytes
for this schema, so a deliberate gap can cost less without changing capacity.

### Recommendation

Keep production unchanged for now. The experiment supports a constant,
alignment-preserving gap added *outside* the capacity-scaled layout, rather than
an implicit extra-capacity rule. For this 64-byte-aligned schema the candidate
formula is `base + blocks * column_block_offset + column_ordinal * gap`.
Any future generator implementation must preserve stronger leaf alignments too.

Do not select 64 bytes solely because it is the smallest tested gap. It eliminated
the wide slowdown at every tested capacity, but its initial narrow result at
131,072 was 12.6% above the fastest gap, missing the predefined 10% criterion.
That observation was not independently repeated and does not establish a real
64-byte-specific regression. The smallest gap passing the initial cross-capacity,
both-kernel criterion was 192 bytes (worst ratio 1.069); 384 bytes also performed
well (worst ratio 1.052). Those small differences are insufficient to rank them
as universal policies.

No fixed gap has yet satisfied the full repeated, cross-capacity acceptance rule:
the staged selection repeated different winners at different capacities. The next
focused experiment should compare 0, 64 and 192 bytes at all five capacities,
with a fixed logical CPU to reduce scheduling variation, before changing generator
defaults. If 64 bytes then passes the criterion, prefer its lower overhead;
otherwise assess 192 bytes. A second CPU and an optimized/vectorized kernel would
be useful validation before broad production adoption, without blocking this
small follow-up. Do not infer a portable cache-layout guarantee from this one host.

## Fixed-CPU and schema confirmation

The opt-in `single-allocation-soa-spacing-confirmation` workflow tests the
sequential rule directly: align the first column, place its capacity elements,
add the nominal gap, then align the next column. There is no trailing gap.
Column alignment is `max(64, alignof(Element))`; the allocation uses the maximum
column alignment. Production owners and generator defaults remain unchanged.

```powershell
# Preparation and correctness checks, including the single-sample pipeline smoke:
cmake --preset benchmark
cmake --build --preset benchmark
ctest --preset single-allocation-soa-spacing-confirmation-correctness

# After stopping competing work:
cmake --build --preset single-allocation-soa-spacing-confirmation

# Complete workflow, including preparation:
cmake --workflow --preset single-allocation-soa-spacing-confirmation
```

The default is processor group 0, logical CPU 2. Override it through CMake with
`cmake --preset benchmark -DSANDBOX_SOA_SPACING_CPU_GROUP=0 -DSANDBOX_SOA_SPACING_CPU=4`,
or use the runner's `--suite confirmation --cpu-group 0 --cpu 4` options.
The benchmark pins its thread before allocation and calibration, checks its
effective affinity and observed CPU before and after measurement, and restores
the original affinity. Invalid or unavailable selections fail; they never silently
fall back. Other workloads, SMT siblings and CPU frequency remain uncontrolled.

Every case processes 65,535 live rows for four passes, with 0-, 64- or 192-byte
nominal gaps. EntityData covers capacities 65,536, 65,600, 75,008, 100,032 and
131,072. Three additional generated experimental shapes cover 65,536, 75,008 and
131,072:

| Schema | Columns | Narrow / wide work |
|---|---:|---|
| SpacingDoubles | 24 | One / four destination-source pairs of nested double vectors |
| SpacingMixedWidths | 42 | One / six nested bundles of 1-, 2-, 3-, 4- and 8-byte leaves |
| SpacingAligned | 15 | One / three nested bundles containing floats and alignas(32/64/256) leaves |

All fields in the selected bundles are exercised. Integer updates use defined
unsigned wraparound or bitwise operations; double and aligned-leaf operations
are checked against ordinary generated owners. Each schema has one shared
non-inlined kernel for all its layouts. Compare layouts within a schema, not
absolute times across these different workloads.

There are 96 cases per repetition: 84 spaced layouts/kernels, ten generated
EntityData controls, and two TArray controls. Three independently shuffled
repetitions use fresh processes and 100 Catch2 samples each: **288 processes**.
The preparation smoke uses 28 single-sample cases at capacity 65,536 and cannot
select a policy. The usual fast comparison does not include this suite.

In addition to the original artifacts, each run records schema and field paths,
layout policy, actual CPU before/after timing, a layout identity derived from
relative column offsets/extents/alignments, and effective inter-column gaps.
`extra_bytes` records total padding; `gap_extra_bytes` records extra bytes over
the same schema's sequential zero-gap layout. Over-aligned columns can require
extra alignment padding. Identical whole layouts produced by different nominal
gaps share evidence; partial similarities do not collapse distinct layouts.

`confirmation-summary.csv` retains each nominal case's median/minimum/maximum
process mean. `confirmation-decision.json` checks complete three-process,
100-sample coverage and applies the 10% criterion to the medians. It prefers 64,
then 192, otherwise neither. It separately lists groups whose process means
differ by more than 10%, requiring review before accepting a candidate. Identical
effective layouts use a median over their combined independent process means;
confidence intervals are never pooled. Incomplete and smoke runs cannot select
a candidate. PNGs show every process and its own bounds, separately by schema.

For stronger alignment, a sequential zero-gap layout need not match the former
capacity-scaled block layout. An additional AlignmentData correctness check
demonstrated this distinction. Archived generated controls are labelled
`generated-blocks`; experimental layouts are labelled `sequential`. Current
generated controls use the selected 192-byte policy, and correctness checks now
require the generated and experimental 192-byte layouts to match exactly.

### Fixed-CPU results, 9 September 2026

**192 bytes was selected and is now the generated default.** Both gaps eliminate the EntityData power-of-two pathology, but 64
does not meet the cross-schema acceptance criterion. This is evidence from one
machine and these kernels, not a guarantee for every schema or processor.

The full run completed all 288 isolated processes on an AMD Ryzen 9 9950X3D,
Windows 11, Unreal Development, using Mimalloc. Every process verified group 0,
logical CPU 2 before and after timing. Each table entry below is the median of
three independent process means, each measured with 100 Catch2 samples. All
layouts process 65,535 live rows for four passes; allocation and initialization
are outside the timed region.

EntityData wide iteration, milliseconds:

| Capacity | Zero gap | 64-byte gap | 192-byte gap |
|---:|---:|---:|---:|
| 65,536 | 7.265 | 1.409 | 1.409 |
| 65,600 | 1.432 | 1.407 | 1.408 |
| 75,008 | 1.446 | 1.409 | 1.408 |
| 100,032 | 1.436 | 1.410 | 1.410 |
| 131,072 | 7.367 | 1.390 | 1.409 |

With 192-byte gaps, the two power-of-two cases improve by about 5.2x. Its
EntityData narrow medians range from 0.430 to 0.438 ms; zero-gap medians range
from 0.435 to 0.458 ms. The generated owner controls reproduce the slow layouts:
wide medians are 7.267 and 7.393 ms at the two power-of-two capacities. The
65,536-row-capacity TArray control measures 6.889 ms wide and 0.456 ms narrow.

Comparison with the original TArray results is retained below. The original
65,536-live-row timings were supplied in the investigation discussion; the
fixed-CPU run uses 65,535 live rows. Values from different sessions are shown
separately because affinity, system conditions and live count differ.

| Session and owner/layout | Narrow | Wide |
|---|---:|---:|
| Original: TArray | 0.419 ms | 6.938 ms |
| Original: Single/Mimalloc, unpadded | 0.412 ms | 7.566 ms |
| Original: Single/Mimalloc, reserve 64 extra rows | 0.396 ms | 1.324 ms |
| Fixed CPU: TArray control | 0.456 ms | 6.889 ms |
| Fixed CPU: Single/Mimalloc, zero-gap experimental layout | 0.450 ms | 7.265 ms |
| Fixed CPU: Single/Mimalloc, 192-byte experimental gaps | 0.437 ms | 1.409 ms |

Within the original session, reserving 64 extra rows improved wide iteration by
5.24x against TArray (80.9% less time), and narrow by about 5.5%. Within the
fixed-CPU session, deliberate 192-byte gaps improve wide iteration by **4.89x
against TArray (79.5% less time)**, and narrow by about **4.0%**. Unpadded single
storage was instead about 9.1% slower than TArray wide in the original session,
and 5.5% slower in the fixed-CPU experimental layout. The benefit is therefore
from the chosen spacing in this workload, not simply from using one allocation.
The fixed gap adds 9,984 bytes to this 53-column layout at any tested capacity;
the original extra-capacity workaround added approximately 12.5 KiB.

Additional shapes, wide iteration in milliseconds:

| Schema | Capacity | Zero gap | 64-byte gap | 192-byte gap |
|---|---:|---:|---:|---:|
| Doubles | 65,536 | 11.405 | 6.205 | 6.052 |
| Doubles | 75,008 | 6.265 | 6.081 | 6.183 |
| Doubles | 131,072 | 11.401 | 6.085 | 6.187 |
| Mixed widths | 65,536 | 19.546 | 19.062 | 19.055 |
| Mixed widths | 75,008 | 19.089 | 19.071 | 19.098 |
| Mixed widths | 131,072 | 19.321 | 19.081 | 19.103 |
| Over-aligned | 65,536 | 5.679 | 5.687 | 5.711 |
| Over-aligned | 75,008 | 5.714 | 5.677 | 5.675 |
| Over-aligned | 131,072 | 5.698 | 5.681 | 5.993 |

Doubles reproduce a substantial power-of-two effect. Mixed-width and
over-aligned wide kernels do not reproduce the 5x effect, despite their zero-gap
column bases also sharing one page offset at power-of-two capacities. Address
correlation alone is therefore not sufficient to predict a slowdown: the
access pattern, element widths and generated instructions matter.

Across all 28 schema/capacity/kernel combinations, 192 is at worst 5.48% above
the fastest tested median and 5.17% above zero-gap. Its largest relative cost is
over-aligned wide iteration at capacity 131,072. The 64-byte candidate is at
worst 38.67% above both comparisons: over-aligned **narrow** iteration at
131,072 takes 2.678 ms, versus 1.931 ms with either zero or 192-byte gaps.

Variability matters here. That failing 64-byte case has process means of 3.240,
1.925 and 2.678 ms, so it is not uniformly slow. The corresponding 192-byte
case measures 1.926, 1.931 and 2.085 ms. Seven of the 96 case groups have a
maximum/minimum process-mean ratio above 1.10. One is the 192-byte double-wide
case at 131,072: 6.178, 6.187 and 7.266 ms. Its median passes, but that slower
process prevents claiming that 192 removes every intermittent slowdown.
CPU affinity does not control physical-page placement, competing work on SMT
siblings, frequency or other system activity. Three processes per case provide
useful confirmation, not a detailed distribution of rare events.

At power-of-two capacities, both nonzero gaps give every column a distinct
4 KiB offset in these four schemas; their measured relative layouts remain
distinct. This rules out using only the count of distinct page offsets to
explain the difference between 64 and 192. Assembly for each shared kernel and
its compiler arguments were archived locally. No hardware counters were
collected, and these measurements do not distinguish cache-set conflicts,
address aliasing, TLB effects or other microarchitectural causes.

The tested sequential policy aligns each column to `max(64, alignof(T))`, places
`capacity * sizeof(T)` bytes, adds the nominal gap before the next column, then
aligns that column. The allocation uses the maximum column alignment. There
is no gap after the final column. Total added bytes, constant across the tested
capacities:

| Schema | 64-byte nominal gap | 192-byte nominal gap |
|---|---:|---:|
| EntityData, 53 columns | 3,328 | 9,984 |
| Doubles, 24 columns | 1,472 | 4,416 |
| Mixed widths, 42 columns | 2,624 | 7,872 |
| Over-aligned, 15 columns | 1,280 | 2,816 |

Over-aligned effective gaps are 64/256 or 192/256 bytes respectively; all
others equal their nominal gap. The 256-byte leaf alignment is preserved.
For EntityData, choosing 192 over 64 costs only another 6,656 bytes.

**Decision implemented after this run:** the generated single-allocation path
uses the tested 192-byte sequential policy, retaining normal alignment and
capacity quantisation, with no power-of-two branches or runtime heuristics.
Generated layout correctness checks compare every column and the exact extent
against the experimental reference for all four shapes plus AlignmentData,
at small, power-of-two and non-power-of-two capacities. Existing compact views
remain 16 bytes. Production fighter storage remains TArray-backed.

The saved timings above measure the experimental layout before this generator
change; a fresh timed comparison of generated output has not been run. Keep
the confirmation suite available for future schema/toolchain changes. New runs
label generated controls `generated-192` with gap 192; archived `generated-blocks`
controls describe the former zero-gap owner. The zero-gap experimental comparison
is still available without retaining a second production layout policy.

Validation before timing: benchmark build and formatting passed; 12 targeted
codegen tests passed; all five confirmation correctness/pipeline tests passed
(including 28 smoke processes); six report unit tests passed; Pyright reported
no errors in the three Python files. Correctness covers alignment through 256
bytes, non-overlap, gap sentinels, nested views and kernel results against the
ordinary generated owners.

The complete local archive is
`.local/benchmarks/single-allocation/spacing/20260908-235359` (started 8 September,
finished 9 September local time). Repository snapshots retain
[process timings](results/fixed-cpu-spacing-2026-09-09/timings.csv),
[column addresses and strides](results/fixed-cpu-spacing-2026-09-09/columns.csv),
[all median/min/max results](results/fixed-cpu-spacing-2026-09-09/confirmation-summary.csv),
[decision and variability checks](results/fixed-cpu-spacing-2026-09-09/confirmation-decision.json)
and machine/binary metadata. PNGs show individual process means and bounds:

| Schema | Narrow | Wide |
|---|---|---|
| EntityData | [PNG](results/fixed-cpu-spacing-2026-09-09/EntityData-narrow.png) | [PNG](results/fixed-cpu-spacing-2026-09-09/EntityData-wide.png) |
| Doubles | [PNG](results/fixed-cpu-spacing-2026-09-09/SpacingDoubles-narrow.png) | [PNG](results/fixed-cpu-spacing-2026-09-09/SpacingDoubles-wide.png) |
| Mixed widths | [PNG](results/fixed-cpu-spacing-2026-09-09/SpacingMixedWidths-narrow.png) | [PNG](results/fixed-cpu-spacing-2026-09-09/SpacingMixedWidths-wide.png) |
| Over-aligned | [PNG](results/fixed-cpu-spacing-2026-09-09/SpacingAligned-narrow.png) | [PNG](results/fixed-cpu-spacing-2026-09-09/SpacingAligned-wide.png) |
