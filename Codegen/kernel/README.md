# SandboxCore kernel DSL

`kernelc` generates concrete array-kernel overloads from deliberately narrow S-expression
declarations. The language describes operation semantics, operand storage, concrete types, public
variants, aliasing contracts, and named C++ emission profiles. It does not evaluate Lisp or accept
arbitrary C++ bodies or ABI spellings.

## Grammar

```text
document      := kernel_module+ EOF
kernel_module := "(" "kernel-module" identifier module_item+ ")"
module_item   := emit | type_set | map | sum
emit          := "(" "emit" ("unreal" | "standard" | "unreal-avx2-lab" | "native-x86-simd-lab") emit_item+ ")"
emit_item     := header | source | avx512_source | dispatch_source | tests | header_include | namespace | export | select | soaos | vector3_groups
header        := "(" "header" quoted_path ")"
source        := "(" "source" quoted_path ")"
avx512_source := "(" "avx512-source" quoted_path ")"
dispatch_source := "(" "dispatch-source" quoted_path ")"
tests         := "(" "tests" quoted_path ")"
header_include := "(" "header-include" quoted_path ")"
namespace     := "(" "namespace" qualified_identifier ")"
export        := "(" "export" identifier ")"
select        := "(" "select" selection_item+ ")"
selection_item := "(" "operation" identifier ")"
                | "(" "type" concrete_type ")"
                | "(" "storage" ("array" | "scalar")+ ")"
                | "(" "variant" ("out-of-place" | "in-place" | "sum") ")"
soaos         := "(" "soaos" "16" ")"
vector3_groups := "(" "vector3-groups" vector3_group+ ")"
vector3_group := "(" identifier identifier identifier identifier ")"
type_set      := "(" "type-set" identifier concrete_type+ ")"
concrete_type := "int32" | "uint32" | "float" | "double"
map           := "(" "map" identifier map_item+ ")"
map_item      := types | operand | output | expression | variants | aliasing
sum           := "(" "sum" identifier sum_item+ ")"
sum_item      := types | operand | expression | aliasing
types         := "(" "types" identifier ")"
operand       := "(" "operand" identifier storage ")"
storage       := "array" | "scalar" | "(" ("array" | "scalar")+ ")"
output        := "(" "output" identifier ")"
expression    := "(" "expression" expr ")"
expr          := identifier | decimal | constant
               | "(" ("+" | "-" | "*" | "/") expr expr ")"
decimal       := signed decimal integer, fraction, or base-10 exponent
constant      := "(" "constant" ("nan" | "infinity" | "negative-infinity") ")"
variants      := "(" "variants" variant+ ")"
variant       := "(" "out-of-place" identifier ")"
               | "(" "in-place" operand_name identifier ")"
aliasing      := "(" "aliasing" ("output-disjoint" | "pairwise-disjoint") ")"
```

A `map` evaluates its expression once per element and writes an output element. A `sum` evaluates
its expression once per element and adds the values into one scalar result, starting at zero. The
initial reduction form is intentionally narrow: it accepts only `float` array operands, requires
`pairwise-disjoint`, and is available only to `native-x86-simd-lab`. It is enough to describe a dot
product without introducing arbitrary loop bodies, configurable identities, or a general reduction
language.

The default aliasing policy is `output-disjoint`: a separate output may not overlap an input, and
an in-place target may not overlap another array operand. Read-only inputs may alias each other.
Every array in one generated overload has the same length. Empty views are valid.

Storage choices form a Cartesian product. An all-scalar out-of-place combination is discarded, an
in-place target is forced to array storage, and declaration order determines C++ argument order.
No commutative or associative rewriting is performed.

Finite decimal literals are validated against every concrete type used by the operation and are
rendered with an explicit cast to that type. Non-finite values use the explicit `constant` form and
require a type-set containing only `float` and/or `double`; bare `nan` and `inf` are ordinary operand
identifiers.

The `unreal` profile emits `TArrayView`/`TConstArrayView`, Unreal integer names, checks, owning
`TArray` conveniences, and `RESTRICT` raw kernels. The `standard` profile emits `std::span`, maps
`int32` and `uint32` to the corresponding `<cstdint>` types, and has no Unreal dependencies. A
standard emission may also request a generated GoogleTest source. Each generated overload is
exercised at empty, scalar, SIMD-boundary, and larger lengths against values produced by a typed
AST evaluator that is independent of C++ expression rendering. Profile selection is fixed in the
generator rather than configurable through arbitrary C++ strings.

The SIMD lab profiles are deliberately test-only. They select one concrete, pairwise-disjoint
`float` variant. `unreal-avx2-lab` accepts only out-of-place maps and emits an AVX2 autovectorized
baseline plus single-loop and four-way-unrolled intrinsic implementations for the private Unreal
benchmark module.
`native-x86-simd-lab` additionally emits an isolated AVX-512 translation unit for the opt-in native
CMake benchmark, accepts the narrow `sum` reduction described above, and emits a forced-scalar
reference loop. A `(dispatch-source ...)` is optional; the current experiment deliberately omits it
so backend selection remains a caller policy. The vector renderer accepts only operand references,
addition, and multiplication, uses unaligned loads and stores for flat arrays, and disables FMA
contraction. These profiles exist to measure whether explicit backends add value before any SIMD
surface is added to SandboxCore.

The native lab also accepts the deliberately narrow emission field `(soaos 16)`. It derives one
concrete `alignas(64) FloatChunk16` containing only `std::array<float, 16> values`. Every array
operand and result is a separate restricted pointer to an array of these chunks; logical size is a
property of the owning collection, not repeated in every chunk. Chunk kernels accept only
`chunk_count` and always process all 16 lanes, so a partial final chunk contains deliberately valid
padding. No other width or profile accepts this field. This is effectively an array of short SOAs,
and keeps storage selection separate from the operation expression without inventing an
operation-specific struct or a general layout DSL.

For out-of-place float maps, `(vector3-groups (name x y z) ...)` groups selected scalar-array
operands into logical three-float vectors. The native lab then emits three overloads from the same
expression: separate component pointers, packed 12-byte XYZ records, and 16-row blocks with
separate aligned X/Y/Z arrays. This field requires `(soaos 16)`, and every selected operand must
belong to exactly one group.

Sum reductions default to `(floating-point-modes strict)`. A reduction may explicitly request
`(floating-point-modes strict relaxed)` to generate both an ordered source loop and a second source
loop that may be reassociated by the compiler. Relaxed loops are emitted into dedicated translation
units so the build applies the relaxed policy only to those functions. FMA contraction remains
disabled in both modes, keeping reassociation separate from contraction as a benchmark dimension.

## Native SIMD benchmark

The native benchmark measures three generated kernels:

```text
out[i] = base[i] + value[i] * scale
dot_product = sum(lhs[i] * rhs[i])
out[i] = dot(lhs_3d[i], rhs_3d[i])
```

Dot product is a useful second target because it combines vector arithmetic with a reduction. A
single accumulator forms a dependency chain; the four-way-unrolled AVX2 version uses four
independent vector accumulators to test whether breaking that chain repays the extra code. SIMD
reductions change the order of floating-point additions, so the dot-product correctness tests use a
high-precision scalar reference and a relative tolerance rather than requiring bitwise equality.

The scalar add and reduction operations are measured over two generated layouts. `flat` is the
existing pointer-and-count API, including scalar tails. `chunked16` uses separate arrays of
`FloatChunk16` for each operand and result. Its kernels use aligned AVX2 or AVX-512 loads and stores
and always process all 16 lanes of every chunk, with no scalar element tail. Dot-product padding is
zero-filled so padded products are neutral; `add_scaled` padding is ordinary storage and is
deliberately processed. This is an
experimental contract, not a public SandboxCore API. The generated autovec source disables
vectorization of the outer chunk loop while leaving the fixed 16-lane inner loop available; relaxed
dot product accumulates 16 lanes across chunks and reduces those lanes once after the loop.

The 3D dot-product map is the focused vector-layout comparison. `aos` uses packed XYZ records,
`soa-flat` uses six independent component pointers like `FVectors3f`, and `soa-chunked16` uses
aligned blocks containing 16 Xs, 16 Ys, and 16 Zs per operand. It registers only matching autovec
and explicit AVX2/AVX-512 backends at 4,096, 16,384, 65,536, and 100,000 vectors, plus the scalar
AOS loop as the natural naïve baseline.

Google Benchmark is the timing harness. The executable registers each case with
`benchmark::RegisterBenchmark`, links `benchmark::benchmark_main`, and lets Google Benchmark
calibrate iteration counts, run repetitions, interleave cases, and write the result JSON. The
Python/Matplotlib script does not execute or time kernels; it only reads that JSON and produces the
plots.

### Generated implementation locations

The native implementations being benchmarked are concrete generated `.cpp` files. They are not
committed source files. CMake runs `sandbox-kernelc --profile native-x86-simd-lab` and writes them
under the selected preset's build tree:

```text
out/build/<preset>/Codegen/kernel/native-simd-generated/native/generated/
  add_scaled_x86_simd_lab.h
  add_scaled_x86_simd_lab_avx2.cpp
  add_scaled_x86_simd_lab_avx512.cpp
  dot_product_x86_simd_lab.h
  dot_product_x86_simd_lab_avx2.cpp
  dot_product_x86_simd_lab_avx512.cpp
  dot_product_x86_simd_lab_relaxed_avx2.cpp
  dot_product_x86_simd_lab_relaxed_avx512.cpp
  dot_product_3d_x86_simd_lab.h
  dot_product_3d_x86_simd_lab_avx2.cpp
  dot_product_3d_x86_simd_lab_avx512.cpp
```

Every native benchmark build also refreshes a stable convenience mirror, independent of the active
build preset:

```text
out/benchmarks/kernel/generated/
```

This is a regular mirrored directory rather than a symbolic link, so it works on Windows without
Developer Mode or elevated link privileges.

For the plotting preset, `<preset>` is `kernel-benchmark-plots`. The AVX2 translation unit contains
the strict scalar/autovec functions and relaxed explicit AVX2 functions. The AVX-512 translation
unit contains strict autovec and relaxed explicit AVX-512 functions, including the `_mm512_*`
intrinsic loops. The two relaxed translation units contain only their respective relaxed-autovec
loops.

CMake compiles those exact generated files into separate strict AVX2, strict AVX-512, relaxed AVX2,
and relaxed AVX-512 object libraries and links the objects into `kernel-native-benchmarks`. The
relaxed targets enable reassociation while disabling contraction;
the remaining targets retain source-order floating-point compilation. The harnesses in
`Codegen/kernel/benchmarks/add_scaled_benchmarks.cpp` and
`Codegen/kernel/benchmarks/dot_product_benchmarks.cpp` call the resulting functions. The focused
layout comparison is in `Codegen/kernel/benchmarks/dot_product_3d_benchmarks.cpp`. The semantic
declarations in `Plugins/SandboxCore/Source/SandboxCore/Kernels/candidate_math.sbxkernel` and the
renderer in `Codegen/kernel/src/avx2_lab_renderer.cpp` are the committed sources of truth.

Native output is kept under `out/` because it depends on the CMake compiler and ISA configuration;
the directory is Git-ignored. Run either native benchmark workflow before inspecting it. By
contrast, the Unreal AVX2 lab output is committed under
`Plugins/SandboxCore/Tests/SandboxCoreBenchmarks/Private/generated/` because UnrealBuildTool
consumes that checked generated source directly.

Benchmark names have the form:

```text
<operation>/<policy>/<layout>/<backend>/<value-set>/<alignment>/<element-count>
```

For example, `add_scaled/elementwise/flat/avx2/ordinary/unaligned/4096` runs the explicit AVX2 kernel on
4,096 elements using the ordinary finite-value data set, with every array starting four bytes past
a 64-byte boundary. `add_scaled/elementwise/chunked16/avx2/ordinary/aligned/4096` runs the
corresponding chunk kernel over 256 complete chunks.

The backends are:

* `scalar`: the generated scalar reference loop. With Clang, loop vectorization, interleaving, and
  unrolling are explicitly disabled for this loop; with MSVC, vectorization is explicitly disabled.
  It is compiled in the same AVX2 translation unit as the AVX2 comparisons, so this controls loop
  structure rather than forcing an obsolete x86 instruction encoding;
* `autovec-avx2`: the generated `add_scaled` scalar loop, compiled in an AVX2 translation unit and
  left to the compiler's vectorizer;
* `autovec-avx2` and `autovec-avx512`: compiler-vectorized source loops. Under the dot-product
  `strict` policy these preserve source accumulation order and may remain scalar; under `relaxed`
  they are compiled in dedicated translation units that permit reassociation but prohibit FMA
  contraction;
* `avx2`: the generated single-loop AVX2 intrinsic implementation;
* `avx2-unrolled`: the generated AVX2 implementation with four vector operations emitted per loop
  iteration;
* `autovec-avx512`: the generated `add_scaled` scalar loop compiled in an AVX-512 translation unit;
* `avx512`: the generated single-loop AVX-512 intrinsic implementation;
* `avx512-unrolled`: the generated dot-product AVX-512 implementation with four independent vector
  accumulators;

The generated functions use ordinary overload names inside selectable namespaces, such as
`backend::avx2::add_scaled` and `relaxed::backend::avx512::dot_product`. A caller can select a fixed
default with a `using` declaration or store one overload in a function pointer. Runtime CPUID
dispatch is outside this timing matrix; `cpu-features` is used only by the harness to skip AVX-512
cases on unsupported machines.

The `autovec` names describe the source and compilation target, not a guarantee that the compiler
selected a particular vector width. The separate `scalar` backend makes that distinction measurable
even if a compiler changes its vectorization decisions. The benchmark JSON records
`absolute_error` and `relative_error` counters for every dot-product result against a
double-precision reference. Inspect the optimized assembly when the exact emitted instructions
matter.

All kernel pointer arguments are `RESTRICT` and the benchmark supplies separate allocations for
every flat array argument. Chunk functions likewise receive a separate restricted pointer for each
operand and output chunk array. The two flat alignment cases are:

* `aligned`: each array begins on a 64-byte boundary;
* `unaligned`: each array begins one `float`, or four bytes, past a 64-byte boundary.

The explicit intrinsic implementations use unaligned loads and stores in both cases because the
kernel API does not promise alignment. The cases measure the effect of the actual starting address;
they do not select different load or store instructions. `FloatChunk16` arrays are always 64-byte
aligned and their intrinsic functions use aligned loads and stores, so no `unaligned` chunk cases
are generated.

The value sets are deterministic:

* `ordinary` uses finite arithmetic sequences for `base` and `value`, with `scale` fixed at `-0.75`;
* `extreme` cycles through positive and negative zero, `1`, `-1`, minimum normal values, maximum
  finite values, denormals, positive and negative infinity, and NaN.

Dot product uses its own finite `ordinary` sequences. It deliberately omits `extreme`: a reduction
quickly collapses most mixtures containing infinity and NaN to NaN, which adds cases without giving
a useful performance comparison.

Deterministic values make runs reproducible. Input values are not randomized because `add_scaled`
has no data-dependent branches. Both timed reports enable Google Benchmark's random interleaving,
which changes the order in which benchmark cases are sampled across repetitions and helps reduce
systematic frequency, temperature, and ordering bias.

Ordinary cases cover element counts around the AVX2 and AVX-512 widths, representative gameplay
workloads, larger cache regimes, and arrays up to 1,048,576 elements. Extreme-value `add_scaled`
cases use 32, 256, 4,096, 65,536, and 1,048,576 elements to sample small, L1, L2, and shared-cache
regimes without duplicating the entire matrix. Every case is run for seven randomly interleaved
repetitions with a minimum of 0.05 seconds per repetition.

The routine `kernel-benchmark-plots` report selects ordinary, aligned cases at 4,096, 16,384,
65,536, and 100,000 elements for both layouts. These are the default representative workload sizes.
The report includes the `elementwise` add and `relaxed` dot-product policies: 96 cases with a
configured timing floor of about 34 seconds. This is the default iteration report; benchmark filters
should be narrowed further when only one operation, layout, or backend changed.

The larger 262,144- and 1,048,576-element cases are optional stress/scaling measurements for
examining streaming-memory behaviour. They remain registered and can be selected through the
explicit `kernel-benchmark-plots-full` workflow or a direct Google Benchmark filter; they are not
part of the routine report.

The `kernel-benchmark-plots-full` report runs all 738 cases: 396 for `add_scaled`, 306 for relaxed
dot product, and 36 for strict dot product. Its configured timing floor is about 258 seconds, so
allow roughly four and three-quarter minutes plus build and plotting time. It covers SIMD-width
boundaries and scalar tails for flat arrays, both flat alignments, and the `add_scaled` extreme set.
Chunk timings deliberately sample only 16/16, 11/16, and 8/16 final-chunk occupancy—no waste,
approximately one-third waste, and half waste—rather than duplicating the broad correctness matrix.
Flat 8-, 11-, and 16-element cases provide direct layout references for those occupancies. Run it
before accepting a backend, layout, or numerical-policy change.
Unsupported AVX-512 cases are reported as skipped.

Allocation and input initialization occur outside the timed loop. The timed body calls the kernel,
prevents its result from being optimized away, and uses real elapsed time. Reported `add_scaled`
byte throughput counts two `float` reads and one `float` write per element; the scalar `scale` is
not included. Dot-product throughput counts its two `float` reads per element. Standard
`items_per_second` and `bytes_per_second` use the requested logical element count so layouts compare
useful work. Chunk cases additionally record `physical_items_per_second` and `padding_fraction`,
making the extra processed lanes visible for partial blocks.

The plotting script uses repetition medians for the curves and repetition standard deviation for
the time error bars. It writes separate backend plots for each layout and applicable alignment, a
flat unaligned-to-aligned time-ratio plot, and a direct chunked-over-flat layout-speedup plot for each
value set. A ratio above `1.0` on an alignment plot means the unaligned case was slower. A ratio
above `1.0` on a layout plot means the chunked layout was faster than the same flat backend. Backend
speedup plots use `scalar` as the baseline where present. Relaxed dot product has no misleading
scalar member; its plots use `autovec-avx2` as their baseline. Strict and relaxed reductions are
separate plot categories rather than mixed into one backend legend.

## Commands

Kernel manifests explicitly order their input documents:

```lisp
(kernel-manifest
  :schema-version 1
  :entries ("array_math.sbxkernel" "candidate_math.sbxkernel"))
```

Manifest input paths are relative to the manifest and must use the `.sbxkernel` extension.

```text
kernelc --manifest <path> --profile <unreal|standard|unreal-avx2-lab|native-x86-simd-lab> [--output-root <directory>] [--check]
cmake --build --preset codegen --target generate-kernel-code
cmake --build --preset codegen --target check-generated-kernel-code
cmake --build --preset codegen --target generate-kernel-avx2-lab
cmake --build --preset codegen --target check-generated-kernel-avx2-lab
cmake --workflow --preset kernel-benchmark
cmake --workflow --preset kernel-benchmark-plots
cmake --workflow --preset kernel-benchmark-plots-full
cmake --workflow --preset kernel-vector-layout-benchmark-plots
out/build/kernel-benchmark/Codegen/kernel/kernel-native-benchmarks.exe --benchmark_repetitions=5 --benchmark_enable_random_interleaving=true
```

The `kernel-benchmark` workflow builds the native benchmark and runs its SIMD correctness tests and
Google Benchmark dry run. It does not run a timed matrix. Both plotting workflows require `uv`,
write Google Benchmark JSON to `out/benchmarks/kernel/results.json`, and use the locked Matplotlib
environment declared by `Scripts/plot-kernel-benchmarks.py` to write headless PNG reports under
`out/benchmarks/kernel/plots`. The most recently run workflow owns this single canonical report.
Each workflow clears the old results and plots before timing, which prevents stale full-report plots
from surviving a happy-path run and makes an open-image file lock fail before the benchmark starts.
Do not run the two report workflows concurrently. Matplotlib and its dependencies are not part of
the native or Unreal build graphs. PNG is also the plotter's default for direct invocations; pass
`--format=svg` when a vector report is preferred.
