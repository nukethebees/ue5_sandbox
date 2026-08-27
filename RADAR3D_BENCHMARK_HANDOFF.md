# Radar3D benchmark handoff

## Goal

Add a benchmark to the existing Radar3D RDG-to-Slate experiment, following the pattern used by the
Heatmap experiment. The benchmark should prove how the fixed 512x512 RDG renderer scales with
contact count.

## Work completed

The benchmark implementation has been added but has not yet passed a compile because verification
was interrupted while the CMake build was waiting for UnrealBuildTool's global lock.

### Benchmark runner

New files under `Plugins/SandboxUI/Source/Experiments/Private/Benchmarks/Radar3D/`:

- `Radar3DBenchmark.h`
- `Radar3DBenchmark.cpp`
- `Radar3DBenchmarkRDG.cpp`
- `Radar3DBenchmarkCommandlet.h`
- `Radar3DBenchmarkCommandlet.cpp`
- `README.md`

The runner uses deterministic synthetic arrays of 1, 4, 16, 64, and 256 contacts. Contact counts
are capped at 256 because the experimental compute shader loops over every contact for every
512x512 output pixel.

It reports these stages in microseconds:

- `api_submission`: game-thread contact snapshot allocation/copy and render-command enqueue.
- `gpu_upload_compute`: absolute-time RHI query around the RDG structured-buffer upload,
  transitions, and compute dispatch.

Reports include minimum, median, p95, maximum, and sample count. CSV columns are:

```text
stage,contacts,samples,min_us,median_us,p95_us,max_us
```

### Renderer support

Modified:

- `Plugins/SandboxUI/Source/Experiments/Private/Radar3D/Radar3DRenderer.h`
- `Plugins/SandboxUI/Source/Experiments/Private/Radar3D/Radar3DRenderer.cpp`

The normal RDG graph construction was factored into `execute_radar_graph`. A benchmark-only
`measure_radar_3d_gpu` function now brackets that graph with two `RQT_AbsoluteTime` queries and
returns `TOptional<double>` in microseconds. The normal widget/render path is otherwise unchanged.

### Showcase wiring

Modified:

- `Plugins/SandboxUI/Source/Experiments/Public/Experiments/Radar3D/Radar3DShowcase.h`
- `Plugins/SandboxUI/Source/Experiments/Private/Radar3D/Radar3DShowcase.cpp`
- `Plugins/SandboxUI/Source/Experiments/Private/Radar3D/README.md`

The Radar3D Editor Utility Widget now has a **Benchmark RDG contact scaling** button and a read-only
results box. The interactive run uses 2 warmup and 10 measured iterations.

### Commandlet

The new commandlet is `Radar3DBenchmark`. Intended invocation:

```text
UnrealEditor-Cmd.exe Sandbox.uproject -run=Radar3DBenchmark -AllowCommandletRendering \
  -RenderOffscreen -unattended \
  -ContactCounts=1,4,16,64,256 -Warmup=10 -Iterations=100 \
  -Output=Saved/Benchmarks/Radar3DBenchmark.csv
```

It rejects Null RHI and supports `ContactCounts`, `Warmup`, `Iterations`, and `Output` arguments.

## Verification state

- The changed C++ files were run through the project's Windows clang-format executable.
- Windows Git `diff --check` passed.
- A DebugGame build was started with:

```text
cmake --build --preset debug-game
```

It printed:

```text
Build.bat is already running, waiting for existing script to terminate...
```

The build then remained queued with no further output. A process check did not show an obvious
UBT/compiler process, so this may have been a stale Unreal build mutex or an unrelated build that
ended without releasing promptly. The user interrupted the session before this was resolved.

No real-RHI commandlet run or post-change CTest run has happened yet.

## Recommended next steps

1. Check for a still-running build/editor process. Do not kill user processes without permission.
2. Run the DebugGame build through the CMake layer:

   ```text
   cmake --build --preset debug-game
   ```

3. Fix any compile/UHT errors. Likely review points if compilation fails:

   - Whether `TConstArrayView<FRadar3DContact>` may use the forward-declared contact type in
     `Radar3DBenchmark.h`; include `Radar3D/Radar3DRenderer.h` there if Unreal's templates require a
     complete type.
   - Whether ignored-return warnings or RHI query APIs differ under this exact UE 5.8 build.
   - Whether the native commandlet class is discovered as `Radar3DBenchmark` after UHT runs.

4. Run a small real-RHI smoke benchmark, for example with 1 and 4 contacts and one measured sample,
   using the same offscreen UnrealEditor command pattern used for shader validation. Confirm:

   - `FRadar3DCS` compiles for `PCD3D_SM6`.
   - The commandlet exits successfully.
   - The CSV contains both timing stages when timestamp queries are supported.

5. Run the project test workflow after the C++ build succeeds:

   ```text
   cmake --workflow --preset debug-game-tests
   ```

6. As the final step, regenerate project files once, as required by the root `AGENTS.md`:

   ```text
   cmake --workflow --preset generate-project-files
   ```

7. Re-run Windows Git status/diff checks and review all untracked benchmark files before handing
   off.

## Existing Radar3D context

The underlying Radar3D experiment was already implemented and previously built successfully. A
real D3D12 offscreen launch compiled `FRadar3DCS` without diagnostics after renaming HLSL function
parameters named `point` to `sample_position`. `point` is an HLSL input-primitive modifier and had
caused the misleading `modifiers must appear before type` error plus cascading parse errors.

That lesson is recorded in:

- `Plugins/SandboxUI/AGENTS.md`

Do not undo that shader naming fix. The Radar3D experiment deliberately uses no level, world,
actor, component, SceneCapture, or hidden mini-world.

## Working-tree caution

The repository was already a shared/dirty worktree during this work. Preserve unrelated user
changes. At the last status check, the files listed above were the benchmark-related modifications
and `Plugins/SandboxUI/Source/Experiments/Private/Benchmarks/Radar3D/` was untracked. Other earlier
Radar3D experiment files and assets may already be present as separate user/session changes.
