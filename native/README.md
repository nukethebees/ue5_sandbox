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

Standalone presets set `SANDBOX_WITH_UNREAL=OFF`, so they do not require `UE_ROOT`. Configure and
build with a native workflow, for example:

```powershell
cmake --workflow --preset win-x64-clangcl-debug
```

Use the corresponding `-unity` preset for faster full builds, or an `-msvc` preset for MSVC.
Native tests and tools use static Tracy; Unreal-enabled builds additionally publish the shared-Tracy
simulation artifacts below `Binaries/Native/`.

See [Native clang-tidy](../docs/clang-tidy.md) for the opt-in clang-cl tidy workflow.

Native dependencies are pinned submodules under `native/third_party` and are initialized by
`csetup`. For a manual clone setup, initialize the required submodules with Git before configuring.

```powershell
git submodule update --init native/third_party/googletest native/third_party/cpu_features `
  native/third_party/benchmark native/third_party/cli11 native/third_party/nlohmann_json
```

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
