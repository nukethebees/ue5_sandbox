# Native libraries and tools

`native/` contains standalone C++ libraries, simulation code, code-generation tools, and their
tests. Prefer working here when Unreal Engine integration is unnecessary; Unreal-facing modules
link the resulting artifacts through thin adapters.

## Layout

- `core/`, `memory/`, `image/`, `mesh_gen/`, and `profiling/`: reusable native systems.
- `simulation/` and `simulation_benchmark/`: worldless combat simulation, tests, and runners.
- `level_authoring/` and `s7/`: authored scenario support.
- `lispb/`: schema, Slate, kernel, and material generation; see its [local guide](lispb/README.md).
- `sbx_mimalloc/`: project allocator integration.
- `third_party/`: pinned dependencies; do not treat their documentation as project guidance.

## Standalone workflow

Standalone presets set `SANDBOX_WITH_UNREAL=OFF`, so they do not require `UE_ROOT`. The default
native configuration uses Windows clang-cl Debug + unity:

```powershell
cmake --preset native
cmake --build --preset native --target native-simulation-tests
ctest --preset native-simulation-tests

# Full first-party native validation.
cmake --workflow --preset native-tests
```

Mimalloc validation builds its small `NativeBinaryTools` host dependency into the native build
tree when needed. Native workflows do not require a prior `ctools` run.

Use `native-core-tests` for the core-focused workflow. The detailed matrix presets remain available
for ASan, non-unity, release, and MSVC selection; `native` is the ordinary fast default.
Native tests and tools use static Tracy; Unreal-enabled builds additionally publish the shared-Tracy
simulation artifacts below `Binaries/Native/`.

See [Native clang-tidy](../docs/clang-tidy.md) for the opt-in clang-cl tidy workflow.

Native dependencies are pinned submodules under `native/third_party` and are initialized by
`csetup`. For a manual clone setup, initialize the required submodules with Git before configuring.

```powershell
git submodule update --init native/third_party/googletest native/third_party/cpu_features `
  native/third_party/benchmark native/third_party/cli11 native/third_party/nlohmann_json
```

The optional memory layout planner additionally uses the pinned SDL3 and Dear ImGui submodules:

```powershell
git submodule update --init native/third_party/sdl native/third_party/imgui
cmake --workflow --preset layout-planner
```

The executable remains local to the worktree at
`%REPOSITORY_ROOT%\out\build\layout-planner\tools\layout_planner\app\layout-planner.exe`; it is not
installed system-wide or added to `PATH`. Run it from the repository root to load
`lispb/project.lispb`, or pass `--project` and `--target` explicitly. See the
[memory layout planner guide](../tools/layout_planner/README.md) for usage, supported analysis, and
V1 limitations.

Simulation tests live in `simulation/tests/`. Asset/configuration conversion and presentation remain
covered by Unreal tests. See [Build and test](../docs/build-and-test.md) for the full test workflows
and [Code generation](../Codegen/README.md) for generated outputs.

To run a deterministic S7 level as a native benchmark, use
`Scripts/run-native-simulation-benchmark.ps1`; the fighter and frame-memory runners build on the
same tool. See [Benchmarks](../docs/benchmarks.md) for supported workloads and
[Profiling](../docs/profiling.md) for Tracy capture.

The Windows clang-cl ASAN workflows exclude the code-generation error-path tests that inspect
caught C++ exceptions because the LLVM 21 Windows ASAN runtime terminates while accessing them.
The exclusions are explicit in `cmake/presets/features.py`; the corresponding non-ASAN clang-cl and
MSVC tests remain enabled.

To refresh the checked-in native simulation fixture, build `editor`, then run:

```powershell
ctest --test-dir out/build/debug-game -R '^Sandbox.ExportSimulationFixture$' --output-on-failure
```

This writes `.local/simulation_fixture.cpp`. Review it against
`simulation/tests/support/simulation_fixture.cpp`; native tests never load Unreal assets or refresh
the snapshot automatically.
