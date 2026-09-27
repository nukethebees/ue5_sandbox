# Benchmarks

BenchmarkTools intentionally tracks the checkout. Build its private host output with
`cmake --build --preset native --target benchmark-tools-host` after configuring `native`.
The PowerShell entry points perform that focused build automatically; there is no shared staging.

Benchmarks are opt-in measurements, not ordinary test runs. Complete build and setup work first,
then use the repository runner or benchmark CMake target so the per-user jobserver can wait for
older work and acquire exclusive benchmark and machine resources. Do not start measurements beside
an unmanaged Unreal build or Editor session.

Results are disposable local data unless a specific experiment says otherwise; write them beneath
`.local/benchmarks/` rather than committing them.

## Entry points

| Measurement | Entry point | Notes |
| --- | --- | --- |
| Fighter scheduling simulation | `out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe fighter-simulation` | Default 2,000- and 4,000-fighter cases; writes JSON and summary CSV. The PowerShell name remains a façade. |
| Generic native simulation | `out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe native-simulation` | Shared jobserver-aware C# runner around `native-simulation-benchmark` for an S7 level. |
| Frame-memory level workload | `out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe frame-memory-level` | Uses the batch benchmark scenario. The PowerShell name remains a façade. |
| Revision A/B frame-memory comparison | `out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe frame-memory-revision-ab` | Safely creates and evaluates a detached baseline worktree. |
| Level telemetry | `out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe level-telemetry` | Configures, builds, and runs the telemetry CTest preset. |
| GPU starfield | `out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe gpu-starfield` | Runs, validates, and writes versioned JSON/CSV/Markdown artifacts. |
| SandboxISMC | `out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe sandbox-ismc` | Builds and runs the existing PIE benchmark with owned CSV, log, trace, and conditions artifacts. |
| SandboxISMC revision comparison | `out/build/native/host-tools/BenchmarkTools/Debug/BenchmarkTools.exe sandbox-ismc-revision-ab` | Builds detached baseline/current candidate inputs, then measures complete interleaved repetitions under one reservation. |
| Unreal-backed measurements | Benchmark CMake presets and commandlet targets | Presets are in `cmake/presets/*benchmarks.json`. |

Run the fighter benchmark with:

```powershell
pwsh -NoProfile -File Scripts/run-fighter-simulation-benchmark.ps1
```

Run the initial scaling matrix with:

```powershell
pwsh -NoProfile -File Scripts/run-fighter-simulation-benchmark.ps1 `
    -FighterCaps 1000,2000,4000,8000,16000
```

Run a specific S7 level through the generic level benchmark runner with:

```powershell
.\out\build\native\host-tools\BenchmarkTools\Debug\BenchmarkTools.exe native-simulation `
    --level .\LevelScripts\BenchmarkFleet_10.scm `
    --seconds 20
```

Pass `--fighter-caps`, `--seconds`, `--warmup-seconds`, `--saturation-timeout-seconds`, or
`--output-dir` to the C# command to change the workload or destination. The PowerShell façade maps
its established parameter names. Use `--skip-build` only after confirming the benchmark binary is current.

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

For scripts and result plotters, see the [Scripts guide](../Scripts/README.md). For jobserver
implementation details, see [tools/jobserver](../tools/jobserver/README.md).

PowerShell remains the interactive/report façade, C# owns benchmark orchestration and jobserver
integration, and Python remains for plotting or scientific analysis.

## SandboxISMC experiments

Build the current checkout's orchestrator with the host-tool target above. `UE_ROOT` supplies the
Editor installation, or pass `--editor <Engine/Binaries/Win64/UnrealEditor-Cmd.exe>` explicitly.

```powershell
.\out\build\native\host-tools\BenchmarkTools\Debug\BenchmarkTools.exe sandbox-ismc `
  --instances 1000 --width 1280 --height 720 --warmup-seconds 1 --seconds 2

.\out\build\native\host-tools\BenchmarkTools\Debug\BenchmarkTools.exe sandbox-ismc-revision-ab `
  --baseline <commit-or-ref> --repetitions 2 --width 1280 --height 720 `
  --instances 40000 --mode custom --update-percent 100 --seconds 5
```

Both commands retain the benchmark actor's workload generation, per-frame sampling and summary
statistics. The existing `sandbox-ismc-benchmark` CMake/CTest workflow remains available.
The new commands configure/build its `editor` target before launching the same automation test.
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

PIE requests a fixed scene viewport, observes its actual size on a later tick, and fails before
warmup if it differs. It never writes Editor viewport preferences. Requested and observed sizes,
RHI, shadows, screen percentage, dynamic resolution and workload controls are recorded separately
from revision provenance. Additional engine/GPU/driver metadata can extend the manifest without
changing the workload schema; no hardware inventory is collected today.

Run IDs combine UTC time with a GUID. Each command owns
`.local/benchmarks/<command>/<run-id>/manifest.json`, `measurement-plan.json`, and `sequence.json`.
SandboxISMC process artifacts live beneath `runs/<process-run-id>/`:

```text
manifest.json      # status, source, effective arguments, expected artifacts, failure
metrics.csv        # original Unreal within-run summaries
result.json        # result schema, run ID, completion, observed comparability conditions
unreal.log         # Unreal's explicit absolute log destination
process.log        # captured process stdout/stderr
capture.utrace     # required when --trace 1
```

Manifests and JSON outputs are replaced atomically. Failed runs retain their directory and error.
The parent captures source HEAD, dirty status (including untracked paths), and a tracked diff hash;
the hash does not identify untracked contents. Executable paths and effective arguments accompany
the source identity. A dirty candidate is explicitly marked as such. No artifact is selected by
timestamp, and no files are collected from `Saved/Benchmarks` or a shared Editor log.

Comparisons use the current executable as the outer orchestrator even when the baseline predates
BenchmarkTools. Baseline refs resolve once to exact commits. Owned inputs are detached worktrees
under `.local/benchmarks/worktrees/<run-id>/baseline`; no feature branches are created. Only owned
worktrees are removed, including after failures. `--keep-baseline-worktree` retains an owned input
for inspection. An explicitly supplied baseline must be a clean worktree root at the resolved
commit and is never removed or patched. Candidate source files are never swapped or overwritten.

For a historical SandboxISMC harness lacking the output/viewport protocol, explicitly add
`--compatibility sandbox-ismc-v1`. This applies an embedded, reviewed patch limited to the lab
actor's output/viewport/metadata plumbing and its JSON dependency, in an owned detached baseline.
The exact patch, SHA-256 and original revision are retained. Git checks every hunk before applying;
unsupported historical harnesses fail during preparation. This option cannot run arbitrary
preparation scripts or substitute renderer implementation files. Omit it when both revisions
already support the protocol.

Both revision commands build before reserving exclusive `machine` and `benchmark` resources.
One job holds the reservation across all warmup and measured processes. Measured pairs alternate
AB then BA (ABBA across two pairs); `sequence.json` records the exact ordering. Analysis runs after
the lease releases. Keep actual campaigns within the repository's three-minute benchmark budget.

SandboxISMC writes `captures.json` with each process's original sample count/min/median/p95/max.
`comparison.json`, `.csv`, and `.md` compare distributions of complete-run medians, with nearest-rank
quantiles, absolute deltas and percentage deltas (null for a zero baseline). They never count
adjacent frame samples as independent repetitions. CPU upload, CPU thread/frame, GPU, submission
bytes, waits and churn metrics retain distinct identities and units. Generic pairing rejects
duplicate identities, missing metrics, units/dimensions mismatches and non-finite values. All
observed comparability conditions must match across measured runs; an incomparable result contains
errors and no performance deltas. Provenance such as revision, timestamp and artifact paths may differ.

Frame-memory uses the same ownership, run manifest, ordering and lease infrastructure while retaining
its existing `raw-results.csv` and `paired-results.csv` reports. `--iterations` counts complete
repetitions per side; `--warmup-iterations` produces separate complete warmup runs. Prepared historical
binaries remain supported without requiring that revision's BenchmarkTools commands.

## Related documentation

- [Profiling](profiling.md): capture native level benchmarks with Tracy.
- [Level scripts](../LevelScripts/README.md): find benchmark scenarios and other S7 levels.
- [Scripts](../Scripts/README.md): benchmark runners and analysis utilities.
