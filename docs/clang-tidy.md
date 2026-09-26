# Native clang-tidy

Native clang-tidy uses clang-cl and LLVM's `run-clang-tidy`. Targeted subsystem audits are the
normal developer and agent workflow. Full and scoped audits inherit `native/.clang-tidy`,
including its static-analyzer checks. Simulation adds its nested policy described below.
Unreal Engine, generated sources,
third-party code, `sbx_mimalloc`, compile/rejection fixtures, and the existing specifically
excluded translation units remain outside the audit.

## Run clang-tidy

Generated presets select `LLVM_ROOT=C:/dev/llvm/install`. This single CMake cache path selects
`bin/clang-cl.exe`, `bin/clang-tidy.exe`, `bin/run-clang-tidy`, and `bin/clang-format.exe`, without
depending on PATH ordering. Override it in a local configure preset if necessary.

By default, tidy uses the selected executable directly. The optional machine installation can
contain the C++23 IOJ checks statically linked into `clang-tidy.exe`; no DLL, `-load`, development
packages, or executable import library is required by the game build. Configure reports whether
IOJ checks are available. Ordinary clang-tidy runs the standard checks when they are absent.

Game workflows never build or repair LLVM. Building custom tooling is an intentional
infrastructure task: see [the LLVM build instructions](../tools/llvm/README.md). The installed
checker is a machine-level snapshot, not necessarily the current worktree's checker source.
When changing a checker, explicitly rebuild it and run its semantic tests before installing it.
Agents doing unrelated work must use the available tool rather than locate, clone, or rebuild LLVM.

Run the scope affected by your change:

```powershell
cmake --workflow --preset clang-tidy-core
cmake --workflow --preset clang-tidy-simulation
```

Run multiple scopes sequentially when a change crosses subsystem boundaries. For shared-header
changes, include consuming subsystems as appropriate; scopes select translation units by directory
and do not automatically discover dependents or inspect Git changes.

| Preset | Native directories |
| --- | --- |
| `clang-tidy-core` | `core`, `profiling` |
| `clang-tidy-simulation` | `simulation`, `simulation_benchmark` |
| `clang-tidy-layout` | `layout` |
| `clang-tidy-lispb` | `lispb` |
| `clang-tidy-memory` | `memory` |
| `clang-tidy-level-authoring` | `level_authoring` |
| `clang-tidy-s7` | `s7` |
| `clang-tidy-image` | `image` |
| `clang-tidy-mesh-gen` | `mesh_gen` |

Shaders has no eligible C++ translation units and therefore no tidy scope.

For a local Windows Debug database measured when these scopes were introduced, the full filter
selected 306 unique translation units: core 49, simulation 116, layout 26, LispB 95, memory 4,
level authoring 5, S7 3, image 5, and mesh generation 3. Counts depend on the configured source
set; these are selection counts, not runtime measurements.

Every workflow reuses the single `win-x64-clangcl-debug-tidy` configure preset and compilation
database at `out/build/win-x64-clangcl-debug/clang-tidy`. Only source selection and the log name
vary. This tree disables precompiled headers and C++ dependency scanning so entries can be
analyzed independently without build-only module-map response files.

LispB and full audits automatically prepare their required generated build-tree inputs,
including in a fresh tree. Up-to-date generated outputs are reused on subsequent runs.

For a comprehensive whole-native audit:

```powershell
cmake --workflow --preset win-x64-clangcl-debug-tidy
```

To rerun a scope against the configured database, use its build preset:

```powershell
cmake --build --preset clang-tidy-core
```

The underlying CMake targets are `native-clang-tidy` and `native-clang-tidy-<scope>`.
Findings are not fatal and fixes are not applied automatically; inspect diagnostics before
considering validation complete. Output is saved in the shared build tree as `clang-tidy.log`
for full audits or `clang-tidy-<scope>.log` for scoped audits. Each run overwrites only its own log.

The audit uses `run-clang-tidy`'s detected CPU count by default. Choose a different worker count
while configuring, for example:

```powershell
cmake --preset win-x64-clangcl-debug-tidy -D IOJ_CLANG_TIDY_JOBS=4
cmake --build --preset clang-tidy-core
```

Set `IOJ_CLANG_TIDY_JOBS=0` to restore the automatic worker count.

The enabled checks and audit compiler arguments are defined in `native/.clang-tidy`.

## Simulation policy

`native/simulation/.clang-tidy` inherits the native policy and enables exactly one custom check,
`ioj-loop-condition-call`. It rejects calls in C-style `for` conditions, including member calls.
Hoist stable bounds into const locals. It does not inspect initializers, increments, loop bodies,
range-for, `while`, or `do` conditions, and supplies no automatic fix.

For deliberately changing conditions, use a local suppression with a reason:

```cpp
// NOLINTNEXTLINE(ioj-loop-condition-call) -- advancing can finish the timeline.
for (SimTick tick{}; tick < maximum_ticks && !timeline.is_finished(); ++tick) {
    advance(tick_period);
}
```

`simulation_benchmark` is included in the execution scope but does not inherit this custom policy.
The existing translation-unit diagnostic scope is explicit because LLVM 24 changed its default
header filter. When IOJ checks are available, run the registration, semantic and nested-policy tests:

```powershell
ctest --test-dir out/build/win-x64-clangcl-debug/clang-tidy -L clang-tidy --output-on-failure
```

## DLL comparison mode

`IOJ_CLANG_TIDY_USE_DLL=ON` preserves the previous per-worktree DLL build and automatic `-load`
workflow for comparison. Use a matching DLL-capable LLVM installation, not the static IOJ executable.
This mode still requires `include/clang`, `include/clang-tidy`, LLVM/Clang CMake packages,
`LLVM_EXPORT_SYMBOLS_FOR_PLUGINS=ON`, `CLANG_PLUGIN_SUPPORT=ON`, and installed `lib/clang-tidy.lib`.
The baseline's host/runtime matching and two LLVM install fixes remain available; the static path
does not use them. See [the comparison and cleanup list](../tools/llvm/ARCHITECTURE.md).

LLVM `24.0.0git` revision `688a1498b3ce` also needs the local
[analyzer lifetime fix](../cmake/clang_tidy/llvm-patches/control-dependency-node-lifetime.patch).
It prevents a control-dependency visitor from retaining a recycled analyzer node, which crashed
the full audit in `native/lispb/src/packed_value_internal.cpp`. Apply the patch at the LLVM source
root before building/installing LLVM. It includes a C++23 regression test and preserves analyzer
diagnostics; no checks are disabled. The installation at `C:/dev/llvm/install` includes this fix.
