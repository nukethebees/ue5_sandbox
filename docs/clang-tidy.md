# Native clang-tidy

Native clang-tidy uses clang-cl and LLVM's `run-clang-tidy`. Targeted subsystem audits are the
normal developer and agent workflow. Full and scoped audits use the same `native/.clang-tidy`
configuration, including its static-analyzer checks. Unreal Engine, generated sources,
third-party code, `sbx_mimalloc`, compile/rejection fixtures, and the existing specifically
excluded translation units remain outside the audit.

## Run clang-tidy

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
