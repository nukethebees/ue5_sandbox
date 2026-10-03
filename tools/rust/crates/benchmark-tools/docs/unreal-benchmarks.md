# Unreal benchmarks

Follow the [benchmark preparation and measurement workflow](../../../../../docs/benchmarks.md#before-measuring)
for jobs-board coordination. Set `UE_ROOT` to your Editor installation. SandboxISMC also accepts
`--editor <Engine/Binaries/Win64/UnrealEditor-Cmd.exe>`.

## Run SandboxISMC

```powershell
coj benchmark sandbox-ismc `
    --instances 1000 --width 1280 --height 720 --warmup-seconds 1 --seconds 2
```

The command builds the Development editor before running PIE. Pass `--skip-build` to use a
prepared binary. `--timeout-seconds` defaults to 600 per measurement.

## Compare SandboxISMC revisions

Prepare under a shared ticket:

```powershell
coj benchmark sandbox-ismc-revision-ab `
    --baseline <commit> --prepare-only --label "packed transform"
```

End that ticket, obtain an exclusive ticket, and use the printed baseline path:

```powershell
coj benchmark sandbox-ismc-revision-ab `
    --baseline <commit> --baseline-worktree <printed-path> --skip-build `
    --repetitions 2 --instances 40000 --mode custom --update-percent 100 --seconds 5
```

The candidate is your current checkout; local first-party edits are allowed. Supplied baselines
must be clean and at the requested commit. Keep both sources unchanged during the experiment.
Both revisions must support the current SandboxISMC measurement protocol; incompatible revisions
are rejected before building.

Add `--validate-only` for a short setup check: one A/B pair, 0.25-second warmup, and 0.5-second
measurement. It overrides timing/repetition options and makes no performance conclusions. Do not
combine it with `--prepare-only`.

Preparation retains the baseline worktree. Supplied worktrees are never removed. For a combined
build-and-measure run, omit the preparation and reuse options; use `--keep-baseline-worktree` if
you want to retain the newly created baseline. Even with `--skip-build`, measurement starts with
shader/cache preparation, so allow time for the Editor to finish it.

## Choose the workload

These options apply to `sandbox-ismc` and `sandbox-ismc-revision-ab` unless marked otherwise.

| Option | Default | Use |
| --- | --- | --- |
| `--width`, `--height` | 1280, 720 | Set the PIE render-target dimensions. |
| `--instances`, `--update-percent` | 40000, 100 | Set population and the percentage updated each frame. |
| `--mode` | paired | Select `paired`, `custom`, or `engine_ismc`. |
| `--visibility` | all | Select `all`, `half`, or `none`. |
| `--bounds` | calculated | Select `calculated` or `supplied`. |
| `--custom-data` | none | Select `none`, `static`, or `animated`. |
| `--shadows`, `--churn` | 0, 0 | Enable either with `1`. |
| `--min-instances` | min(1000, instances) | Set the minimum live population during churn. |
| `--half-cycle-updates`, `--replacement-percent` | 120, 5 | Set churn controls. |
| `--warmup-updates`, `--warmup-seconds` | 0, 1 | Set warmup before measurement. |
| `--seconds`, `--trace` | 5, 1 | Set duration; use `--trace 0` to omit the Insights capture. |
| `--repetitions`, `--warmup-runs` | 2, 0 | Comparison only: complete measured/warmup processes per side. |
| `--output-dir` | `.local/benchmarks/<command>` | Choose the parent directory for runs. |
| `--label` | empty | Name the experiment in its reports. |

## Read or regenerate results

Open `comparison.md` in the printed comparison directory, or use `comparison.csv` and
`comparison.json`. For a failed run, inspect `manifest.json` and the logs under `runs/` and
`preparation/`. Keep the run directory when sharing or revisiting an experiment.

Regenerate a completed comparison without running Unreal or retaining its source worktrees:

```powershell
coj benchmark sandbox-ismc-report `
    --run-dir .local/benchmarks/sandbox-ismc-revision-ab/<run-id>
```

## Run other Unreal workloads

| Workload | Example | Adjustments |
| --- | --- | --- |
| Level telemetry | `coj benchmark level-telemetry --samples 7` | Development; default timeout 1,200 seconds. |
| Spark rendering | `coj benchmark spark --seconds 10` | Defaults: `--capacity 50000 --sparks-per-hit 96 --impacts-per-frame 100 --warmup-frames 60`. Development; timeout 1,200 seconds. |
| GPU starfield | `coj benchmark gpu-starfield --counts 100000,1000000 --resolutions 1920x1080,3840x2160` | Development; default timeout 2,400 seconds. Add `--size-multipliers 1,4 --camera-modes stationary` for the size matrix. |
| Heatmap | `coj benchmark heatmap` | Set `--resolutions`. |
| Radar | `coj benchmark radar-3d` | Set `--contact-counts`. |
| Scatter | `coj benchmark scatter-3d` | Set `--point-counts`. |
| Volume heatmap | `coj benchmark volume-heatmap-3d` | Use `--warmup`, `--iterations`, and `--output`. |
| Entity overlay | `coj benchmark entity-overlay` | Use `--warmup`, `--iterations`, and `--output`. |

Spark and level telemetry accept `--build-dir` for an existing configured tree, `--skip-build`
for prepared binaries, and `--timeout-seconds`. GPU starfield writes to
`Saved/Benchmarks/GpuStarfield`; use `--timeout-seconds` to change its limit.

The five commandlet workloads default to DebugGame and accept `--warmup`, `--iterations`, and
`--output` in addition to their workload-specific controls. See the
[architecture](ARCHITECTURE.md) for measurement protocols and report formats.
