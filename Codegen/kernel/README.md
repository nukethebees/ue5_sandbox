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
emit_item     := header | source | avx512_source | dispatch_source | tests | header_include | namespace | export | select
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
`native-x86-simd-lab` additionally emits isolated AVX-512 and runtime-dispatch translation units for
the opt-in native CMake benchmark, accepts the narrow `sum` reduction described above, and emits a
forced-scalar reference loop. The vector renderer accepts only operand references, addition, and
multiplication, uses unaligned loads and stores, and disables FMA contraction. These profiles exist
to measure whether explicit backends add value before any SIMD surface is added to SandboxCore.

Sum reductions default to `(floating-point-modes strict)`. A reduction may explicitly request
`(floating-point-modes strict relaxed)` to generate both an ordered source loop and a second source
loop that may be reassociated by the compiler. Relaxed loops are emitted into dedicated translation
units so the build applies the relaxed policy only to those functions. FMA contraction remains
disabled in both modes, keeping reassociation separate from contraction as a benchmark dimension.

## Native SIMD benchmark

The native benchmark measures two generated kernels:

```text
out[i] = base[i] + value[i] * scale
dot_product = sum(lhs[i] * rhs[i])
```

Dot product is a useful second target because it combines vector arithmetic with a reduction. A
single accumulator forms a dependency chain; the four-way-unrolled AVX2 version uses four
independent vector accumulators to test whether breaking that chain repays the extra code. SIMD
reductions change the order of floating-point additions, so the dot-product correctness tests use a
high-precision scalar reference and a relative tolerance rather than requiring bitwise equality.

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
  add_scaled_x86_simd_lab_dispatch.cpp
  dot_product_x86_simd_lab.h
  dot_product_x86_simd_lab_avx2.cpp
  dot_product_x86_simd_lab_avx512.cpp
  dot_product_x86_simd_lab_dispatch.cpp
  dot_product_x86_simd_lab_relaxed_avx2.cpp
  dot_product_x86_simd_lab_relaxed_avx512.cpp
```

For the plotting preset, `<preset>` is `kernel-benchmark-plots`. The AVX2 translation unit contains
the `scalar`, strict-autovec, `avx2`, and `avx2-unrolled` functions. The AVX-512 translation unit
contains the strict-autovec and `avx512` functions, including the `_mm512_*` intrinsic loop. The two
relaxed translation units contain only their respective relaxed-autovec loops. The dispatch
translation unit contains the `cpu-features` selection and cached forwarding function.

CMake compiles those exact generated files into separate strict AVX2, strict AVX-512, relaxed AVX2,
relaxed AVX-512, and dispatch object libraries and links the objects into
`kernel-native-benchmarks`. The relaxed targets enable reassociation while disabling contraction;
the remaining targets retain source-order floating-point compilation. The harnesses in
`Codegen/kernel/benchmarks/add_scaled_benchmarks.cpp` and
`Codegen/kernel/benchmarks/dot_product_benchmarks.cpp` call the resulting functions. The semantic
declarations in `Plugins/SandboxCore/Source/SandboxCore/Kernels/candidate_math.sbxkernel` and the
renderer in `Codegen/kernel/src/avx2_lab_renderer.cpp` are the committed sources of truth.

Native output is kept under `out/` because it depends on the CMake compiler and ISA configuration;
the directory is Git-ignored. Run either native benchmark workflow before inspecting it. By
contrast, the Unreal AVX2 lab output is committed under
`Plugins/SandboxCore/Tests/SandboxCoreBenchmarks/Private/generated/` because UnrealBuildTool
consumes that checked generated source directly.

Benchmark names have the form:

```text
<operation>/<backend>/<value-set>/<alignment>/<element-count>
```

For example, `add_scaled/avx2/ordinary/unaligned/4096` runs the explicit AVX2 kernel on
4,096 elements using the ordinary finite-value data set, with every array starting four bytes past
a 64-byte boundary.

The backends are:

* `scalar`: the generated scalar reference loop. With Clang, loop vectorization, interleaving, and
  unrolling are explicitly disabled for this loop; with MSVC, vectorization is explicitly disabled.
  It is compiled in the same AVX2 translation unit as the AVX2 comparisons, so this controls loop
  structure rather than forcing an obsolete x86 instruction encoding;
* `autovec-avx2`: the generated `add_scaled` scalar loop, compiled in an AVX2 translation unit and
  left to the compiler's vectorizer;
* `autovec-strict-avx2` and `autovec-strict-avx512`: the generated dot-product source loop compiled
  without floating-point reassociation. These preserve source accumulation order and may therefore
  remain scalar;
* `autovec-relaxed-avx2` and `autovec-relaxed-avx512`: the same generated dot-product source loop in
  dedicated translation units that permit reassociation but prohibit FMA contraction;
* `avx2`: the generated single-loop AVX2 intrinsic implementation;
* `avx2-unrolled`: the generated AVX2 implementation with four vector operations emitted per loop
  iteration;
* `autovec-avx512`: the generated `add_scaled` scalar loop compiled in an AVX-512 translation unit;
* `avx512`: the generated single-loop AVX-512 intrinsic implementation;
* `dispatch-avx2` or `dispatch-avx512`: the generated runtime-dispatch entry point, named for the
  backend selected once through `cpu-features`. This includes the cached indirect-call overhead.

The `autovec` names describe the source and compilation target, not a guarantee that the compiler
selected a particular vector width. The separate `scalar` backend makes that distinction measurable
even if a compiler changes its vectorization decisions. The benchmark JSON records
`absolute_error` and `relative_error` counters for every dot-product result against a
double-precision reference. Inspect the optimized assembly when the exact emitted instructions
matter.

All kernel pointer arguments are `RESTRICT` and the benchmark supplies separate allocations for
every array argument. The two alignment cases are:

* `aligned`: each array begins on a 64-byte boundary;
* `unaligned`: each array begins one `float`, or four bytes, past a 64-byte boundary.

The explicit intrinsic implementations use unaligned loads and stores in both cases because the
kernel API does not promise alignment. The cases measure the effect of the actual starting address;
they do not select different load or store instructions.

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

Ordinary cases cover element counts around the AVX2 and AVX-512 widths, larger cache regimes, and
arrays up to 1,048,576 elements. Extreme-value `add_scaled` cases use 32, 256, 4,096, 65,536, and
1,048,576 elements to sample small, L1, L2, and shared-cache regimes without duplicating the entire
matrix. Every case is run for seven randomly interleaved repetitions with a minimum of 0.05 seconds
per repetition.

The routine `kernel-benchmark-plots` report selects ordinary, aligned cases at 32, 256, 4,096,
65,536, 262,144, and 1,048,576 elements. It currently contains 96 cases, has a configured timing
floor of about 34 seconds, and should normally finish in under a minute plus build and plotting
time. Use it while iterating.

The `kernel-benchmark-plots-full` report runs all 646 cases: 322 for `add_scaled` and 324 for dot
product. Its configured timing floor is about 226 seconds, so allow roughly four and a half minutes
plus build and plotting time. It covers SIMD-width boundaries, scalar tails, both alignments, and the
`add_scaled` extreme set. Run it before accepting a backend or dispatch change. Unsupported AVX-512
cases are reported as skipped.

Allocation and input initialization occur outside the timed loop. The timed body calls the kernel,
prevents its result from being optimized away, and uses real elapsed time. Reported `add_scaled`
byte throughput counts two `float` reads and one `float` write per element; the scalar `scale` is
not included. Dot-product throughput counts its two `float` reads per element.

The plotting script uses repetition medians for the curves and repetition standard deviation for
the time error bars. It writes an aligned comparison, an unaligned comparison, and an
unaligned-to-aligned time-ratio plot for each value set. A ratio above `1.0` on an alignment plot
means the unaligned case was slower. Speedup plots use `scalar` as the baseline, so a value above
`1.0` means that backend was faster than the forced-scalar reference loop.

## Commands

```text
kernelc --manifest <path> --profile <unreal|standard|unreal-avx2-lab|native-x86-simd-lab> [--output-root <directory>] [--check]
cmake --build --preset codegen --target generate-kernel-code
cmake --build --preset codegen --target check-generated-kernel-code
cmake --build --preset codegen --target generate-kernel-avx2-lab
cmake --build --preset codegen --target check-generated-kernel-avx2-lab
cmake --workflow --preset kernel-benchmark
cmake --workflow --preset kernel-benchmark-plots
cmake --workflow --preset kernel-benchmark-plots-full
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
