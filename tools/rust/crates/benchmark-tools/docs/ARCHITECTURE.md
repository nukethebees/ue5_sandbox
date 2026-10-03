# Benchmark tools architecture

For commands and workflows, see [Benchmarks](../../../../../docs/benchmarks.md) and
[Unreal benchmarks](unreal-benchmarks.md).

## Responsibilities

`agent-task benchmark` builds the invoking checkout's `benchmark-tools-host` target and delegates
arguments to that executable. Workload orchestration, parsing, comparisons, and reports are
revision-local Rust code; Python remains responsible for kernel plots. The tools do not acquire
jobs-board tickets or resource leases. Callers coordinate builds and measurements.

| Module | Responsibility |
| --- | --- |
| `native.rs` | Native simulation invocation and fighter/frame-memory validation. |
| `compare.rs` | Native commit comparison, literal run-order parsing, and native reports. |
| `revision.rs` | Source fingerprints, owned worktrees, run manifests, and balanced ordering. |
| `results.rs` | Metric identities, summaries, pairing, comparability, and SandboxISMC reports. |
| `frame_revision.rs` | Frame-memory revision comparison and its CSV reports. |
| `ismc.rs` | SandboxISMC preparation, measurement protocol, and offline-report entry point. |
| `tracy.rs` | Offline probe statistics from Tracy exports. |
| `kernel.rs`, `gpu.rs`, `unreal.rs` | Kernel plots and other workload runners. |
| `support.rs` | Argument parsing, process execution, and file-format helpers. |

## Revision inputs and ownership

Comparisons use the invoking checkout's executable as the orchestrator, including when a
historical input predates benchmark-tools. Refs resolve once to exact commits. Native comparison
can isolate both baseline and candidate commits; omitting the candidate ref uses the invoking
checkout and permits local first-party edits.

Owned inputs are detached worktrees under `.local/benchmarks/wt/<number>`. Each slot is reserved
exclusively to keep Windows build paths short. No feature branch is created. Only owned inputs
are removed on normal completion or failure; retention options preserve them. Supplied inputs
must be clean worktree roots at the resolved commits and are never patched or removed. Native
comparison also requires them to be inside the workspace. Candidate sources are not swapped.

Submodule setup can seed from local Git objects and the LFS cache, then checks out each revision's
exact pins. Candidate edits are not copied. Original remote URLs are preserved; missing commits
or LFS assets can be fetched. Native comparison discovers dependencies from each revision's
`native/third_party` gitlinks and builds only the simulation benchmark target after configuring
`native-benchmark`.

## Provenance and source validation

Source identities contain HEAD, dirty status, a tracked binary-diff hash, and a hash of untracked
paths and contents. Symlink targets are hashed instead of their contents. Ignored files and the
owned artifact directory are excluded. Initialized submodules must be clean, including untracked
files, regardless of submodule ignore settings.

Comparisons verify source fingerprints around preparation and measurement. Changed sources fail
the run before publishing deltas; measurements remain for diagnosis. These snapshots neither lock
the checkout nor prove that a prebuilt binary matches its source. `--skip-build` leaves binary
freshness to the caller.

Native comparison requires identical selected level-file bytes and records their hash. This
does not establish equality of every data dependency or simulation behaviour. Historical inputs
must support the build preset, invocation options, and version 2 results. SandboxISMC checks
its run-identity, viewport, and terminal-result protocol in both revisions before building.

## Runs and artifacts

Run IDs combine UTC time, process ID, and a process-local counter. Each run has a unique directory
and a manifest recording configuration, provenance, status, expected artifacts, and failures.
The family directory's `latest.txt` points to the newest created run, including failures. It is
only a navigation aid and never selects measurement inputs. Per-process directories have no
latest pointer. Artifacts are not selected by timestamp or collected from shared Editor logs.

Failures retain their artifact directory. Interrupted writes can leave incomplete data, which
the SandboxISMC offline reporter rejects. Callers own cancellation and wall-clock limits.

Native comparison writes:

| Artifact | Contents |
| --- | --- |
| `manifest.json` | Source identities, arguments, requested sequence, level hash, status, and failures. |
| `preparation.json` | Prepared inputs and arguments for a preparation-only run. |
| `sequence.json` | Measurement order and repetition IDs. |
| `runs/<sequence>-<side>/stdout.jsonl` | Original output, retained before checking exit status. |
| `runs/<sequence>-<side>/stderr.log` | Process diagnostics. |
| `runs/<sequence>-<side>/results.json` | Parsed native results. |
| `captures.json` | Complete-run timing metrics and comparability conditions. |
| `comparison.json`, `.csv`, `.md` | Derived paired reports. |
| `source.diff` | Baseline-to-candidate commit diff. |
| `candidate-working-tree.diff` | Dirty candidate's tracked edits; untracked contents are fingerprinted, not copied. |

SandboxISMC comparisons also record `measurement-plan.json`. Their per-process directories hold
a manifest, original `metrics.csv`, authoritative terminal `result.json`, explicit `unreal.log`,
captured stdout/stderr in `process.log`, and `capture.utrace` when tracing is enabled. The terminal
envelope includes the schema, run ID, completion/error, and observed conditions.

## Ordering and statistics

Balanced ordering alternates AB then BA across complete pairs. Native `--order` instead parses
a literal A/B sequence, case-insensitively, requiring equal counts of 1–100 per side. Each side's
occurrence number is its repetition ID, so the nth A pairs with the nth B even in grouped orders.
An explicit sequence replaces the repetition-count option. Warmup processes in specialised
revision runners are separate from measured repetitions.

Each comparison sample is a complete run, not an adjacent frame or tick. Native captures store
mean/median/p95/p99 tick time, throughput, and realtime factor as distinct scalar metrics.
SandboxISMC captures retain each process's sample count, minimum, median, p95, and maximum;
comparisons operate on those complete-run medians.

Reports preserve baseline/candidate distributions and compute paired candidate-minus-baseline
differences. Summary medians average the middle pair for even counts; p95 uses nearest rank.
Percentage distributions are null if any paired baseline is zero, while individual pairs retain
defined percentages. The reports do not establish statistical significance.

Metric identity includes name, unit, and dimensions. Pairing rejects duplicate identities,
missing metrics, mismatched units/dimensions, and non-finite values. Observed conditions must
match across measured runs; otherwise reports contain errors and no performance deltas. Revision
IDs, timestamps, and paths are provenance and may differ. CPU upload, CPU thread/frame, GPU,
submission bytes, waits, and churn remain distinct metrics.

SandboxISMC offline reporting validates the manifest, sequence, and captures, then regenerates
the three reports through the same comparison code. Raw captures remain unchanged. Incomplete
runs, unsupported schemas, and older captures missing required render conditions are rejected.

Frame-memory comparisons share ownership, manifests, and ordering while retaining
`raw-results.csv` and `paired-results.csv`. Iterations represent complete repetitions per side;
warmup iterations are separate processes. Prepared historical binaries need not provide their
revision's benchmark-tools commands.

## Native fighter workload

The workload waits for exact fighter saturation, applies a post-saturation warmup, and measures
steady-state simulation. Laser damage is disabled and ship health is raised to maintain population
while normal fighter behaviour and collision work remain active.

Validation requires stable population, attacking state, active firing, no replacement spawns,
and no frame-memory overflow. Native comparisons also check schema, completed/measured ticks,
game speed, timings, and matching observed workload/build/profiler conditions.

Multiple caps execute in one native process in requested order. A Tracy capture therefore contains
every case, each surrounded by a named zone such as `Fighter simulation benchmark: 4000 fighters`.
The generic Rust runner accepts comma-separated stress caps; the native executable also accepts
separate cap values.

## SandboxISMC preparation and measurement

SandboxISMC configures `development`, builds `editor`, and launches automation directly. The actor
owns workload generation, per-frame sampling, and within-run statistics. Both revisions are
prepared before the complete warmup/measurement sequence.

Each revision first loads the benchmark map in the normal Editor, completes shader/asset
compilation, and exits without starting PIE. Both share the invoking checkout's persistent
`.local/benchmarks/ddc` cache. Logs live under `preparation/`. Cache preparation also runs with
`--skip-build` and has its own timeout. Preparation-only mode builds and retains the baseline
without launching Unreal. Validation-only mode runs a short pair through the measurement protocol
and marks its reports as unsuitable for performance conclusions.

PIE requests fixed render-target dimensions and observes them on a later tick. A mismatch fails
before warmup without modifying Editor viewport preferences. Workload/render conditions are
separate from provenance: requested/observed sizes, RHI, shadows, frame-limit disablement,
`r.VSync`, `r.VSyncEditor`, `t.MaxFPS`, screen percentage, and dynamic resolution. Owned runs
disable `r.Editor.Viewport.OverridePIEScreenPercentage` and require its observed value to be zero.
Engine/GPU/driver metadata can extend the manifest without changing the workload schema; the
runner does not currently inventory hardware.

The runner reads the terminal envelope before requiring successful measurement artifacts.
Known viewport/setup failures terminate PIE without requiring CSV or trace output. Map-load and
PIE-start failures publish the same envelope even before the actor exists.

## Tracy report processing

The offline reader uses a compatible `tracy-csvexport` from PATH. It streams exporter output
without saving raw CSV. The exporter still loads the capture in memory.

Probes are grouped by static name, source file, and source line across threads; dynamic zone text
is not a grouping key. Name filters are case-sensitive substrings. Thread filters use Tracy
identifiers, which need not be OS thread IDs. Durations/timestamps are nanoseconds. Statistics
include count, total, mean, minimum, exact nearest-rank p50/p95/p99, maximum, and the worst
invocation timestamps and threads.

Default durations include nested zones. Self time subtracts nested work on the same thread.
Time bounds select invocations by start timestamp in `[from, to)`, relative to the capture,
without clipping durations. Warmup is not removed automatically; the native benchmark's
`Benchmark measured ticks` zone identifies the steady-state interval.

Negative-duration unfinished invocations are skipped and counted. Unmatched filters yield an
empty report, not zero-duration samples. Embedded newlines in exported names/text are unsupported.

The reader retains durations and bounded worst-event lists. Its default five-million-event limit
uses about 40 MB for durations, plus capacity and metadata. Exceeding that limit or 4,096 distinct
probes fails without publishing a partial report. `--max-events` changes the event limit;
`--top` only limits output. Defaults are ten probes and three worst invocations each, with
omitted-probe counts.
