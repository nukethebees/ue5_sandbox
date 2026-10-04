# Benchmarks

Use `coj benchmark <operation>` to run a workload or compare revisions. Keep results in
`.local/benchmarks/` and measurement campaigns under three minutes.

## Before measuring

Build and prepare inputs under a shared [jobs-board ticket](../tools/jobserver/README.md).
For measurement, request an exclusive ticket, check it until Ready, and start it immediately
before the command. End it when the command returns, including on failure. Do not measure
alongside an unmanaged Unreal build or Editor session.

Use `--skip-build` only when binaries match the sources you intend to measure. A command that
builds and measures in one invocation needs an exclusive ticket for that invocation.

## Run a native workload

```powershell
# Measure the standard 2,000- and 4,000-fighter cases.
coj benchmark fighter-simulation

# Choose populations and measurement duration.
coj benchmark fighter-simulation --fighter-caps 1000,2000,4000 --seconds 10

# Run an S7 level for 20 simulated seconds.
coj benchmark native-simulation --level LevelScripts/BenchmarkFleet_10.scm --seconds 20

# Measure the batch scenario's frame-memory use.
coj benchmark frame-memory-level --seconds 20
```

The fighter runner defaults to a five-second warmup and ten-second measurement. Adjust
`--warmup-seconds` or `--saturation-timeout-seconds` when needed. Allow enough time for fighters
to start firing; an inactive workload fails validation. Caps must be unique positive integers.

Find fighter results in the printed directory: `summary.csv` for timings and `results.json`
for full results, and `plots/` for SVG tick-latency and throughput charts. Set `--output-dir` to
choose the destination. Native simulation and frame-memory
commands print their JSON results.

For a frame-memory revision comparison, run `coj benchmark frame-memory-revision-ab
--baseline <ref>` under an exclusive ticket. Set `--iterations` and `--warmup-iterations` to
control complete runs; read `raw-results.csv` and `paired-results.csv` in the printed directory.

## Native commit comparisons

First prepare both inputs under a shared ticket:

```powershell
coj benchmark compare --baseline <before> --candidate <after> --prepare-only
```

Then obtain an exclusive ticket and measure using the paths printed by preparation:

```powershell
coj benchmark compare --baseline <before> --candidate <after> --skip-build `
    --baseline-worktree <printed-baseline> --candidate-worktree <printed-candidate>
```

Omit `--candidate` in both commands, and `--candidate-worktree` in the second, to compare against
your current checkout, including local edits. Repeat the same workload options in preparation
and measurement. Supplied worktrees must be clean, at the requested commits, and inside the
invoking workspace; they remain available afterward.

An explicit revision matching the clean current checkout reuses that checkout instead of
creating another worktree. For a dirty checkout or a different commit, preparation creates
a detached input. The printed paths remain valid for `--skip-build`, including the current
checkout when it was reused.

The default workload uses 2,000 and 4,000 fighters, a five-second warmup, a ten-second measurement,
and two repetitions per side in ABBA order. Adjust `--fighter-caps`, `--seconds`,
`--warmup-seconds`, or `--repetitions`. To compare an S7 level, add these options to both commands:

```powershell
--workload native-simulation --level LevelScripts/BenchmarkFleet_10.scm --seconds 5
```

Use the same level file in both revisions. Both revisions must support the native benchmark
build preset and version 2 results; they do not need the comparison command itself.

Choose an exact order with `--order <ORDER>`, such as `ABAB`, `AABB`, or `AABBAB`.
A is baseline and B is candidate. Use equal counts of each (1–100, either case), and omit
`--repetitions` when specifying an order.

Open `comparison.md` in the printed run directory under `.local/benchmarks/compare/`.
Use `comparison.csv` or `comparison.json` for further analysis. Negative timing deltas mean
faster execution; positive throughput deltas mean higher throughput. Deltas pair the first A
with the first B, and so on. These reports do not establish statistical significance.
Comparable runs also generate SVG timing, throughput, and paired-delta charts in `plots/`,
linked from `comparison.md`.

Inspect `runs/` for raw results and errors, `sequence.json` for ordering, and `source.diff` for
the commit diff. Dirty tracked edits also appear in `candidate-working-tree.diff`.

For a combined build-and-measure invocation, omit the preparation and reuse options.
`--keep-worktrees` retains newly created inputs for inspection. Run
`coj benchmark compare --help` for all options.

## Tracy probe reports

Install a compatible `tracy-csvexport` on PATH and [capture a trace](profiling.md). Then inspect
the saved capture without launching the simulation:

```powershell
# Locate the measured interval, excluding saturation and warmup.
coj benchmark tracy-report --trace .local/benchmarks/run.tracy `
    --filter "Benchmark measured ticks"

# Substitute that interval's start/end times and the subsystem being optimized.
coj benchmark tracy-report --trace .local/benchmarks/run.tracy `
    --filter awareness_scan --from-seconds 20 --to-seconds 25 `
    --sort p95 --worst 3 --output .local/benchmarks/awareness.json
```

Compare the same workload, time window, and timing mode. Durations include nested zones by
default; add `--self` to subtract nested work on the same thread. Use an enclosing phase's
inclusive duration to assess parallel latency rather than summing worker durations.

Use `--top` and `--worst` to adjust report size, or `--thread` to select a Tracy thread identifier.
If an export exceeds the event limit, narrow the filters or raise `--max-events`. Keep raw
traces as local artifacts and inspect compact reports. Use a shared ticket for expensive
offline exports; reserve exclusive tickets for measurements.

## Kernel reports

Build the executables and run their smoke/plot tests under a shared ticket:

```powershell
cmake --workflow --preset kernel-benchmark
```

Then run a report under an exclusive ticket:

```powershell
coj benchmark kernel-report --workload representative --skip-build
```

Choose `representative`, `vector-layout`, `full`, or `highway`. Adjust `--repetitions` and
`--min-time` (seconds) to keep the campaign short. The first three default to seven repetitions
and 0.05 seconds per case, writing results and plots to `out/benchmarks/kernel/`. Highway defaults
to three repetitions and 0.02 seconds, writing `.local/benchmarks/highway/kernels.json`.
All four workloads automatically write SVG charts to `plots/` beside their JSON results.

## Regenerate plots

Plotting runs after measurement using the Rust tool's SVG renderer. A plotting failure warns
without invalidating saved benchmark results. Regenerate charts without measuring again:

```powershell
coj benchmark plot --kind kernel --input out/benchmarks/kernel/results.json --output-dir .local/benchmarks/kernel-plots
coj benchmark plot --kind fighter --input <run>/results.json --output-dir <run>/plots
coj benchmark plot --kind comparison --input <run>/comparison.json --output-dir <run>/plots
```

Kernel charts include throughput, time per element with sample-deviation bars, backend and
layout speedups, and alignment penalties where matching measurements exist. `--baseline`
selects a kernel backend (default `scalar`, falling back to `autovec-avx2`). If both are absent,
absolute charts remain available and backend speedup is omitted with a warning.

Offline plotting returns failure for invalid input, incomparable results, or rendering errors.
Only SVG output is supported; Python and Matplotlib are not needed for plotting.

## SandboxISMC experiments

Follow the [Unreal benchmark guide](../tools/rust/crates/benchmark-tools/docs/unreal-benchmarks.md)
to prepare and compare SandboxISMC revisions, regenerate a report, or run GPU starfield, spark,
level telemetry, and commandlet benchmarks. It includes options and output locations.

## Related documentation

- [Architecture](../tools/rust/crates/benchmark-tools/docs/ARCHITECTURE.md): execution, provenance,
  result formats, and comparison rules.
- [Profiling](profiling.md): capture native benchmarks with Tracy.
- [Level scripts](../LevelScripts/README.md): choose benchmark scenarios and other S7 levels.
