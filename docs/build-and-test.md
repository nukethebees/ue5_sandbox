# Build and test

Use CMake for builds and tests; use `coj` for preparation, editor launches and asset commands.
Build affected targets and run focused tests. Run expensive checks once on the final candidate.

## Setup

Install CMake 4.4.2+, Ninja, ccache, and the MSVC/Windows SDK versions pinned in
`Config/DefaultEngine.ini`. The maintainer installs shared tools and adds them to PATH.

| Variable | Purpose |
| --- | --- |
| `UE_ROOT` | Unreal Engine installation. |
| `IOJ_ROOT` | Shared tools and temporary files. |
| `LLVM_ROOT` | Optional LLVM installation; set before configuring. Otherwise use PATH. |

```powershell
coj prepare-worktree
```

Run once per fresh session. Preparation retains builds, synchronizes submodules, generates
presets/code and disables Live Coding; it does not run the test suite. Native presets disable
Unreal. Engine-facing headers must compile with MSVC; rebuild affected native dependencies
before Unreal after changing ABI/toolset pins.

## Native iteration

```powershell
cmake --preset native
cmake --build --preset native --target native-simulation-tests
ctest --test-dir out/build/native -L '^native-simulation$' -LE 'soak|compile-contract' --output-on-failure
```

Replace the target and label for your subsystem. Use `ctest -N` to preview selection.
The `native` label covers product libraries; `developer-tool` covers standalone tools.
Run tool tests only when tools or shared tool infrastructure change.

| Scope | Command |
| --- | --- |
| Simulation, including soak | `cmake --workflow --preset native-simulation-full-tests` |
| All native tests, including soak and compile contracts | `cmake --workflow --preset native-tests` |
| Developer tools | `cmake --workflow --preset tool-tests` |
| Generated inputs | `coj presets --check`; build target `check-generated-code` |
| Changed C++ formatting | `coj format --changed` |

For Rust changes, select the affected package's CTest entry. Build-layer changes also need
relevant `cmake` tests. [Clang-tidy](clang-tidy.md) and [ASAN](../tools/llvm/README.md)
are opt-in, not normal validation requirements.

## Unreal integration

Build Unreal for engine-facing code, UI, reflection, assets or editor changes:

```powershell
cmake --preset debug-game
cmake --build --preset debug-game --target editor
ctest --test-dir out/build/debug-game -R '^Sandbox.UnitTests$' --output-on-failure
```

Use `development` for Development builds. `debug-game-unit-tests` is the combined build/test
workflow. To run all project editor automation in one process:

```powershell
ctest --preset debug-game-all-unreal-tests
```

This reuses the current editor build and selects `Sandbox.AllEditorTests`, including rendering
and material tests. Other presets exclude that aggregate; unfiltered CTest also runs individual
entries. Benchmarks and native/tool tests are separate.

- **Launch:** `coj editor`; add `--wait-for-debugger` or `--build-dir <directory>` as needed.
- **IDE files:** `coj unreal project-files --build-dir out/build/debug-game`. Select
  `DebugGame Editor | Win64` in Visual Studio.
- **Stale modules:** rebuild the matching editor target; do not edit receipts or manifests.
- **Experiments:** enable `SandboxExperiments` in `Sandbox.uproject`, reconfigure and rebuild
  for shader/UI showcases and `SandboxShaders.MaterialCompilation`.
- **ISMC lab:** enable `SandboxISMCLab` and rebuild for demo maps and `coj benchmark sandbox-ismc`.

## Unreal derived data cache

Editor, automation, cooking and Rust launchers use Unreal's default persistent local Zen DDC.
Unreal manages the server. All worktrees/configurations share it; cache keys separate incompatible
inputs. **Do not clear DDC during ordinary builds, tests or worktree cleanup.**

Set its location externally through Unreal settings or user-level `UE-LocalDataCachePath`;
restart terminals/editors after environment changes. Without an override, Unreal uses its default.
Zen cooked-output storage is a separate setting.

- **Verify:** inspect `Saved/Logs/Sandbox.log` for `LogDerivedDataCache`, `LogZenServiceInstance`,
  the data path and `ZenLocal` with `Status: OK`. Add `-logcmds="LogDerivedDataCache Verbose"`
  for detail; use the editor's Derived Data widget → **View Cache Statistics** for hits/misses.
- **Check reuse:** repeat the same shader workload across processes/worktrees and compare cache
  statistics and compiler-job counts. Duration alone does not prove a cache hit.
- **Prime deliberately:** under a shared jobs ticket, run:

```powershell
& "$env:UE_ROOT/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$PWD/Sandbox.uproject" -run=DerivedDataCache -fill -unattended -nop4 -stdout
```

### Debugging fresh shader compilation

**Cold runs are debugging-only. They are not part of normal development, validation or upgrade
flow.** Use them to investigate suspected compiler execution or cache-related issues.
Normal tests use Zen; unchanged inputs can safely reuse compiled shaders.

Optional `.Cold` entries run the same tests with shader-job reuse disabled and assert that
compiler jobs executed. They do not clear the shared DDC and are not registered by default.

```powershell
cmake --preset debug-game -DIOJ_ENABLE_COLD_SHADER_TESTS=ON
cmake --build --preset debug-game --target editor
ctest --test-dir out/build/debug-game -L '^cold-compilation$' --output-on-failure
cmake --preset debug-game -DIOJ_ENABLE_COLD_SHADER_TESTS=OFF
```

The showcase material test also requires `SandboxExperiments`. See Epic's
[DDC guide](https://dev.epicgames.com/documentation/unreal-engine/using-derived-data-cache-in-unreal-engine)
for engine configuration.

## Packaging and assets

```powershell
cmake --workflow --preset development-staged-game
cmake --workflow --preset development-package
```

`coj run-staged` launches an existing package; select another configuration with `--build-dir`.
Use `coj unreal --help` for asset generation, resaving and audio import. Asset commands build
their prerequisites and default to `out/build/debug-game`.
See [PowerShell](../PowerShell/README.md) for Shipping packaging and configuration-transition checks.

## Caches, profiling and coordination

- **Native compiler cache:** ccache uses user settings; `CCACHE_MAXSIZE` controls capacity.
- **Unreal compiler cache:** CMake starts UBA automatically. Disable with
  `-DIOJ_ENABLE_UBA_CACHE=OFF`; capacity uses `IOJ_UBA_CACHE_CAPACITY_GB`.
- **Build profiling:** use `coj profile-build native` or `coj profile-build unreal`;
  see [profiling commands](../tools/rust/crates/coj/docs/usage.md#build-profiling).
- **Heavy work:** follow the [jobs-board protocol](../tools/jobserver/README.md). Use exclusive
  access for benchmarks and builds writing shared engine output; avoid concurrent editor/UBT runs.
- **Integration:** after checks pass and the maintainer authorizes it, use `coj integrate`.

Details: [native development](../native/README.md), [build architecture](../cmake/ARCHITECTURE.md),
[benchmarks](benchmarks.md), [runtime profiling](profiling.md), [developer tools](../tools/README.md).
