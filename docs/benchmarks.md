# Benchmarks

`agent-task benchmark <operation>` builds and delegates to the checkout’s `benchmark-tools-host`
output. Workloads, comparisons, parsing, and reports stay revision-local.

Benchmarks are opt-in measurements, not ordinary test runs. Complete build and setup work first,
then request an exclusive jobs-board ticket. Check until Ready and explicitly start it before
invoking the agent-task benchmark command. Immediately end it when the command
returns, including failure. Earlier shared tickets finish first; later shared requests wait behind
the benchmark. Do not start measurements beside an unmanaged Unreal build or Editor session.

Results are disposable local data unless a specific experiment says otherwise; write them beneath
`.local/benchmarks/` rather than committing them.

## Entry points

| Measurement | Entry point | Notes |
| --- | --- | --- |
| Fighter scheduling simulation | `agent-task benchmark fighter-simulation` | Default 2,000- and 4,000-fighter cases; writes JSON and summary CSV. |
| Generic native simulation | `agent-task benchmark native-simulation` | Rust runner around `native-simulation-benchmark` for an S7 level. |
| Frame-memory level workload | `agent-task benchmark frame-memory-level` | Uses the batch benchmark scenario. |
| Revision A/B frame-memory comparison | `agent-task benchmark frame-memory-revision-ab` | Safely creates and evaluates a detached baseline worktree. |
| Level telemetry | `agent-task benchmark level-telemetry` | Builds the editor and runs telemetry automation directly; supports `--samples`. |
| GPU starfield | `agent-task benchmark gpu-starfield` | Runs, validates, and writes versioned JSON/CSV/Markdown artifacts. |
| SandboxISMC | `agent-task benchmark sandbox-ismc` | Builds and runs the existing PIE benchmark with owned CSV, log, trace, and conditions artifacts. |
| SandboxISMC revision comparison | `agent-task benchmark sandbox-ismc-revision-ab` | Builds detached baseline/current candidate inputs, then measures complete interleaved repetitions. |
| SandboxISMC offline report | `agent-task benchmark sandbox-ismc-report --run-dir <comparison-run>` | Regenerates reports from captured data without engine runs or comparison worktrees. |
| Kernel reports | `agent-task benchmark kernel-report --workload representative` | Also `vector-layout`, `full`, and `highway`; retains Release builds and Python plots. |
| Spark rendering | `agent-task benchmark spark` | Duration, capacity, sparks/impact, impact count, warmup are command options. |
| Commandlet measurements | `agent-task benchmark heatmap` | Also `radar-3d`, `scatter-3d`, `volume-heatmap-3d`, and `entity-overlay`. |

Run the fighter benchmark with:

```powershell
agent-task benchmark fighter-simulation
```

Run the initial scaling matrix with:

```powershell
agent-task benchmark fighter-simulation `
    --fighter-caps 1000,2000,4000,8000,16000
```

Run a specific S7 level through the generic level benchmark runner with:

```powershell
agent-task benchmark native-simulation `
    --level .\LevelScripts\BenchmarkFleet_10.scm `
    --seconds 20
```

Pass `--fighter-caps`, `--seconds`, `--warmup-seconds`, `--saturation-timeout-seconds`, or
`--output-dir` to the Rust command to change the workload or destination. Use `--skip-build` only after confirming the benchmark binary is current.

Fighter caps must be unique positive 32-bit integers. Results are emitted in the requested cap
order; `results.json` and `summary.csv` retain that order. The generic runner accepts
comma-separated `--fighter-stress-caps` values, while the native `--fighter-stress-caps` option
also accepts separate values.

The fighter runner verifies population stability, attacking state, active firing, absent replacement
spawns, and frame-memory capacity before publishing results. Its outputs include raw details plus
mean, median, p95, p99, tick rate, and realtime factor.

When multiple fighter caps are requested, the runner executes them in one native process. A single
Tracy capture therefore contains every case, with a formatted top-level zone such as `Fighter
simulation benchmark: 4000 fighters` around each one.

The workload waits for exact fighter saturation, applies a post-saturation warm-up, then measures
steady-state simulation. It disables fighter laser damage and raises ship health so the population
remains stable while normal fighter behaviour and collision work remain active.

For scripts and result plotters, see the [Scripts guide](../Scripts/README.md). For coordination,
see the [jobs-board workflow](../tools/jobserver/README.md).

Agent-task is the developer entry point, benchmark-tools owns execution and reporting, and Python
remains for plotting. Benchmark tools have no scheduling responsibilities.

## Measurement options

`spark` defaults to `--seconds 10 --capacity 50000 --sparks-per-hit 96 --impacts-per-frame 100
--warmup-frames 60`. `level-telemetry` defaults to seven samples. Both use Development and retain
their 1,200-second timeout. Pass `--build-dir <configured-tree>` to select an existing build,
`--skip-build` for prepared binaries, or `--timeout-seconds` to change the limit.

`heatmap`, `radar-3d`, `scatter-3d`, `volume-heatmap-3d`, and `entity-overlay` default to DebugGame.
Heatmap accepts `--resolutions`, radar `--contact-counts`, and scatter `--point-counts`; each also
accepts `--warmup`, `--iterations`, and `--output`. Defaults and commandlet output locations are unchanged.

GPU starfield defaults to Development with output in `Saved/Benchmarks/GpuStarfield`. Its former
matrix is `gpu-starfield --counts 100000,1000000 --resolutions 1920x1080,3840x2160
--size-multipliers 1,4 --camera-modes stationary --timeout-seconds 3600`. The ordinary timeout is 2,400 seconds.

Kernel reports accept `--repetitions`, `--min-time` (seconds), and `--skip-build`. Representative,
vector-layout, and full workloads retain seven repetitions, 0.05 seconds per case, and
`out/benchmarks/kernel/results.json` plus plots. Highway retains three repetitions, 0.02 seconds,
and `.local/benchmarks/highway/kernels.json`. Build executables and run smoke/plot tests with
`cmake --workflow --preset kernel-benchmark`; this does not run the report matrix.

## SandboxISMC experiments

The agent-task entry point builds the current checkout’s orchestrator. `UE_ROOT` supplies the
Editor installation, or pass `--editor <Engine/Binaries/Win64/UnrealEditor-Cmd.exe>` explicitly.

```powershell
agent-task benchmark sandbox-ismc `
  --instances 1000 --width 1280 --height 720 --warmup-seconds 1 --seconds 2

agent-task benchmark sandbox-ismc-revision-ab `
  --baseline <commit-or-ref> --repetitions 2 --width 1280 --height 720 `
  --instances 40000 --mode custom --update-percent 100 --seconds 5

# Prepare both binaries and retain the detached baseline without measuring.
agent-task benchmark sandbox-ismc-revision-ab `
  --baseline <commit-or-ref> --prepare-only --label "packed transform"

# A short protocol/comparability smoke, using a prepared clean baseline.
agent-task benchmark sandbox-ismc-revision-ab `
  --baseline <commit-or-ref> --baseline-worktree <retained-path> --skip-build --validate-only

# Re-analyse a completed experiment without Unreal or its source worktrees.
agent-task benchmark sandbox-ismc-report `
  --run-dir .local/benchmarks/sandbox-ismc-revision-ab/<run-id>
```

Both commands retain the benchmark actor's workload generation, per-frame sampling and summary
statistics. The commands configure `development` and build its `editor` target before launching automation directly.
`--timeout-seconds` defaults to 600 for each ISMC measurement.
`--skip-build` uses existing binaries; for comparisons it requires `--baseline-worktree <path>`.
It is the caller's responsibility to ensure those binaries match their recorded sources.

| Option | Default | Meaning |
| --- | --- | --- |
| `--width`, `--height` | 1280, 720 | Requested PIE render-target dimensions |
| `--instances`, `--update-percent` | 40000, 100 | Population and fraction updated per frame |
| `--mode` | paired | `paired`, `custom`, `engine_ismc` |
| `--visibility` | all | `all`, `half`, `none` |
| `--bounds` | calculated | `calculated`, `supplied` |
| `--custom-data` | none | `none`, `static`, `animated` |
| `--shadows`, `--churn` | 0, 0 | Boolean controls, `0` or `1` |
| `--min-instances` | min(1000, instances) | Minimum live churn population |
| `--half-cycle-updates`, `--replacement-percent` | 120, 5 | Churn controls |
| `--warmup-updates`, `--warmup-seconds` | 0, 1 | Warmup within each process, before measurement |
| `--seconds`, `--trace` | 5, 1 | Measured duration and Insights capture toggle |
| `--repetitions`, `--warmup-runs` | 2, 0 | Comparison only: complete measured/warmup processes per side |
| `--output-dir` | `.local/benchmarks/<command>` | Parent directory; every invocation allocates a unique child |
| `--label` | empty | Human-readable manifest/report label; never a path or comparability input |
| `--prepare-only` | off | Comparison only: prepare/build and verify both sources; retain owned baseline without measurement |
| `--validate-only` | off | Comparison only: one A/B pair, 0.25 s warmup and 0.5 s measurement per process, no warmup processes |

Preparation and validation modes are mutually exclusive. Preparation writes `preparation.json`
with effective source identities and prints the retained baseline path. Retained worktrees remain
disposable benchmark-owned state under `.local/benchmarks/wt/`; preparation does not delete
them. Supplied baselines must be clean and remain untouched.

Before measurement, both revisions prepare the benchmark map's shaders and derived data through
the normal Editor. It loads the benchmark map,
finishes asset compilation, and exits without starting PIE. This warms the same shader variants
used by measurement. Preparation runs before
measurement and retains logs under the run's
`preparation/` directory. Both sides use the invoking checkout's persistent `.local/benchmarks/ddc`
cache, which survives disposable baseline cleanup. This also runs with `--skip-build`;
`--prepare-only` continues to build and retain the baseline without launching Unreal.

Validation overrides repetition and timing options for a short measurement.
It uses the normal comparison sequence and artifacts, but its reports explicitly make **no
performance conclusions**. Cache preparation has its own timeout and does not consume this limit.

PIE requests a fixed scene viewport, observes its actual size on a later tick, and fails before
warmup if it differs. It never writes Editor viewport preferences. Requested and observed sizes,
RHI, shadows, actual frame-limit disablement, `r.VSync`, `r.VSyncEditor`, `t.MaxFPS`, screen
percentage, dynamic resolution and workload controls are recorded separately
from revision provenance. Additional engine/GPU/driver metadata can extend the manifest without
changing the workload schema; no hardware inventory is collected today.

Run IDs combine UTC time, process ID, and a process-local counter. Each command owns
`.local/benchmarks/<command>/<run-id>/manifest.json`, `measurement-plan.json`, and `sequence.json`.
The family directory's `latest.txt` contains the absolute path of the newest
created run, including failures. It is only a navigation convenience and is never read to select
benchmark artifacts. Per-process `runs/` directories have no pointer.
SandboxISMC process artifacts live beneath `runs/<process-run-id>/`:

```text
manifest.json      # status, source, effective arguments, expected artifacts, failure
metrics.csv        # original Unreal within-run summaries
result.json        # authoritative terminal state: schema, run ID, completion/error, conditions
unreal.log         # Unreal's explicit absolute log destination
process.log        # captured stdout/stderr written when the Editor returns
capture.utrace     # required when --trace 1
```

Failed runs retain their directory and error. Interrupted writes may leave incomplete artifacts;
offline reports reject incomplete runs. The caller owns cancellation and wall-clock limits.
The runner reads the terminal result before requiring successful measurement artifacts. Known
viewport/setup failures terminate PIE promptly and need no CSV or trace to explain the failure.
Map-load and PIE-start failures publish the same terminal envelope even before the actor exists.
The runner captures source HEAD, dirty status, a tracked diff hash, and a hash of untracked paths
and contents (link targets for untracked symbolic links). Ignored files and the exact owned run
directory are excluded. Executable paths and effective arguments accompany the source identity.
A dirty candidate is explicitly marked as such. Both revision commands check those fingerprints
after preparation, before measurement, and after measurement. A changed source
fails the run before comparison deltas are published; measurements remain available for diagnosis.
First-party candidate edits are allowed, but initialized submodules must be clean, including
untracked files. Source checks enforce this regardless of submodule ignore settings.
These snapshots do not lock the checkout or prove that prebuilt binaries match it. No artifact
is selected by timestamp, and no files are collected from `Saved/Benchmarks` or a shared Editor log.

Comparisons use the current executable as the outer orchestrator even when the baseline predates
benchmark-tools. Baseline refs resolve once to exact commits. Owned inputs are detached worktrees
under `.local/benchmarks/wt/<number>`; the next unused slot is reserved exclusively, keeping Windows
build paths short. The manifest records the path independently of the unique artifact run ID.
No feature branches are created. Only owned worktrees are removed, including after failures.
`--keep-baseline-worktree` retains an owned input
for inspection. An explicitly supplied baseline must be a clean worktree root at the resolved
commit and is never removed or patched. Candidate source files are never swapped or overwritten.

Owned baseline submodules are seeded from the invoking checkout's local Git objects and LFS
cache, then checked out at the baseline's exact pinned commits. Candidate edits are not copied.
Original remote URLs are preserved; missing commits or LFS assets are fetched when necessary.

Both SandboxISMC revisions must provide the current run-identity, viewport, and terminal-result
protocol. The runner checks this before building.

Both revision commands build before running the complete warmup and measurement sequence.
They run ordinary processes directly and never request tickets or acquire resource leases.
The caller manages the exclusive jobs-board ticket for the benchmark invocation, ending it when
the command returns. For SandboxISMC, use `--prepare-only` first under a shared ticket, then supply that baseline
with `--skip-build` under the exclusive ticket to keep builds outside the benchmark turn; cache
preparation still runs inside that invocation. Measured pairs alternate AB then BA (ABBA across
two pairs); `sequence.json` records the exact ordering. Analysis follows measurement.
Keep actual campaigns within the repository's three-minute benchmark budget.

SandboxISMC writes `captures.json` with each process's original sample count/min/median/p95/max.
`comparison.json`, `.csv`, and `.md` preserve baseline/candidate distributions of complete-run
medians, but report changes from **paired candidate-minus-baseline run medians**, matched by
repetition ID. Samples count independent complete repetitions. Medians average the two middle
values for even sample counts; p95 uses nearest rank. Percentage distributions are null if any
paired baseline is zero (individual pairs retain defined percentages). They never count
adjacent frame samples as independent repetitions. CPU upload, CPU thread/frame, GPU, submission
bytes, waits and churn metrics retain distinct identities and units. Generic pairing rejects
duplicate identities, missing metrics, units/dimensions mismatches and non-finite values. All
observed comparability conditions must match across measured runs; an incomparable result contains
errors and no performance deltas. Provenance such as revision, timestamp and artifact paths may differ.
Reports include the label, source SHAs/dirty markers, ordering, workload and observed render controls.
Owned SandboxISMC runs disable the Editor's PIE screen-percentage override and require its observed
value to remain zero, alongside the existing screen-percentage and dynamic-resolution controls.
`sandbox-ismc-report --run-dir <path>` validates the manifest, sequence and `captures.json`, then
regenerates the three derived reports through the same comparison code. Raw captures
are unchanged. Incomplete/failed runs or unsupported capture schemas fail clearly; incomparable
captures produce an incomparable report without deltas. Captures must contain the required render
conditions, including Editor VSync and MaxFPS; older captures missing these cannot establish comparability.

Frame-memory uses the same ownership, run manifest, ordering and measurement infrastructure while retaining
its existing `raw-results.csv` and `paired-results.csv` reports. `--iterations` counts complete
repetitions per side; `--warmup-iterations` produces separate complete warmup runs. Prepared historical
binaries remain supported without requiring that revision's benchmark-tools commands.

## Related documentation

- [Profiling](profiling.md): capture native level benchmarks with Tracy.
- [Level scripts](../LevelScripts/README.md): find benchmark scenarios and other S7 levels.
- [Scripts](../Scripts/README.md): benchmark runners and analysis utilities.
