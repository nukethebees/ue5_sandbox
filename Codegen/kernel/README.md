# SandboxCore kernel DSL

`kernelc` generates concrete array-kernel overloads from deliberately narrow S-expression
declarations. The language describes operation semantics, operand storage, concrete types, public
variants, aliasing contracts, and named C++ emission profiles. It does not evaluate Lisp or accept
arbitrary C++ bodies or ABI spellings.

## Grammar

```text
document      := kernel_module+ EOF
kernel_module := "(" "kernel-module" identifier module_item+ ")"
module_item   := emit | type_set | map
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
                | "(" "variant" ("out-of-place" | "in-place") ")"
type_set      := "(" "type-set" identifier concrete_type+ ")"
concrete_type := "int32" | "uint32" | "float" | "double"
map           := "(" "map" identifier map_item+ ")"
map_item      := types | operand | output | expression | variants | aliasing
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

The SIMD lab profiles are deliberately test-only. They select one concrete, pairwise-disjoint,
out-of-place `float` variant. `unreal-avx2-lab` emits an AVX2 autovectorized baseline plus single-loop and
four-way-unrolled intrinsic implementations for the private Unreal benchmark module.
`native-x86-simd-lab` additionally emits isolated AVX-512 and runtime-dispatch translation units for
the opt-in native CMake benchmark. The vector renderer accepts only operand references, addition,
and multiplication, uses unaligned loads and stores, and preserves the expression-tree order
without FMA contraction. These profiles exist to measure whether explicit backends add value before
any SIMD surface is added to SandboxCore.

## Native SIMD benchmark

The native benchmark measures the generated `add_scaled` kernel:

```text
out[i] = base[i] + value[i] * scale
```

Google Benchmark is the timing harness. The executable registers each case with
`benchmark::RegisterBenchmark`, links `benchmark::benchmark_main`, and lets Google Benchmark
calibrate iteration counts, run repetitions, interleave cases, and write the result JSON. The
Python/Matplotlib script does not execute or time kernels; it only reads that JSON and produces the
plots.

Benchmark names have the form:

```text
add_scaled/<backend>/<value-set>/<alignment>/<element-count>
```

For example, `add_scaled/avx2/ordinary/unaligned/4096` runs the explicit AVX2 kernel on
4,096 elements using the ordinary finite-value data set, with every array starting four bytes past
a 64-byte boundary.

The backends are:

* `autovec-avx2`: the generated scalar loop, compiled in an AVX2 translation unit and left to the
  compiler's vectorizer;
* `avx2`: the generated single-loop AVX2 intrinsic implementation;
* `avx2-unrolled`: the generated AVX2 implementation with four vector operations emitted per loop
  iteration;
* `autovec-avx512`: the same scalar source loop compiled in an AVX-512 translation unit;
* `avx512`: the generated single-loop AVX-512 intrinsic implementation;
* `dispatch-avx2` or `dispatch-avx512`: the generated runtime-dispatch entry point, named for the
  backend selected once through `cpu-features`. This includes the cached indirect-call overhead.

The `autovec` names describe the source and compilation target, not a guarantee that the compiler
selected a particular vector width. Inspect the optimized assembly when the exact emitted
instructions matter.

All kernel pointer arguments are `RESTRICT` and the benchmark supplies separate allocations for
`base`, `value`, and `out`. The two alignment cases are:

* `aligned`: each array begins on a 64-byte boundary;
* `unaligned`: each array begins one `float`, or four bytes, past a 64-byte boundary.

The explicit intrinsic implementations use unaligned loads and stores in both cases because the
kernel API does not promise alignment. The cases measure the effect of the actual starting address;
they do not select different load or store instructions.

The value sets are deterministic:

* `ordinary` uses finite arithmetic sequences for `base` and `value`, with `scale` fixed at `-0.75`;
* `extreme` cycles through positive and negative zero, `1`, `-1`, minimum normal values, maximum
  finite values, denormals, positive and negative infinity, and NaN.

Deterministic values make runs reproducible. Input values are not randomized because `add_scaled`
has no data-dependent branches. The full report instead enables Google Benchmark's random
interleaving, which changes the order in which benchmark cases are sampled across repetitions and
helps reduce systematic frequency, temperature, and ordering bias.

Ordinary cases cover element counts around the AVX2 and AVX-512 widths, larger cache regimes, and
arrays up to 1,048,576 elements. Extreme-value cases use 32, 256, 4,096, 65,536, and 1,048,576
elements to sample small, L1, L2, and shared-cache regimes without duplicating the entire matrix.
Every case is run for seven randomly interleaved repetitions with a minimum of 0.05 seconds per
repetition. On an AVX-512-capable machine this currently produces 276 cases and takes roughly 100
seconds; timing varies with the CPU and machine load. Unsupported AVX-512 cases are reported as
skipped.

Allocation and input initialization occur outside the timed loop. The timed body calls the kernel,
prevents the output from being optimized away, and uses real elapsed time. Reported byte throughput
counts two `float` reads and one `float` write per element; the scalar `scale` is not included.

The plotting script uses repetition medians for the curves and repetition standard deviation for
the time error bars. It writes an aligned comparison, an unaligned comparison, and an
unaligned-to-aligned time-ratio plot for each value set. A ratio above `1.0` on an alignment plot
means the unaligned case was slower. Speedup plots use `autovec-avx2` as the baseline, so a value
above `1.0` means that backend was faster than the AVX2 autovectorized loop.

## Commands

```text
kernelc --manifest <path> --profile <unreal|standard|unreal-avx2-lab|native-x86-simd-lab> [--output-root <directory>] [--check]
cmake --build --preset codegen --target generate-kernel-code
cmake --build --preset codegen --target check-generated-kernel-code
cmake --build --preset codegen --target generate-kernel-avx2-lab
cmake --build --preset codegen --target check-generated-kernel-avx2-lab
cmake --workflow --preset kernel-benchmark
cmake --workflow --preset kernel-benchmark-plots
out/build/kernel-benchmark/Codegen/kernel/kernel-native-benchmarks.exe --benchmark_repetitions=5 --benchmark_enable_random_interleaving=true
```

The `kernel-benchmark` workflow builds the native benchmark and runs its SIMD correctness tests and
Google Benchmark dry run. It does not run the full timed matrix. The `kernel-benchmark-plots`
workflow requires `uv`; it builds and tests the same targets, runs the full timed matrix, writes the
Google Benchmark JSON to `out/benchmarks/kernel/results.json`, and uses the locked Matplotlib
environment declared by `Scripts/plot-kernel-benchmarks.py` to write headless PNG reports under
`out/benchmarks/kernel/plots`. Matplotlib and its dependencies are not part of the native or Unreal
build graphs. PNG is also the plotter's default for direct invocations; pass `--format=svg` when a
vector report is preferred.
