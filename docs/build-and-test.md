# Build and test

CMake owns builds and test registration. `coj` handles worktree preparation, formatting,
editor launches, and asset commands. See [CMake](../cmake/README.md),
[native development](../native/README.md), and [developer tools](../tools/README.md).

## Setup

Install CMake 4.4.2 or newer, Ninja, and the MSVC/Windows SDK versions pinned in
`Config/DefaultEngine.ini`. Set `UE_ROOT` to Unreal and `IOJ_ROOT` to the shared tools root.
The maintainer installs central tools and adds them to PATH; preparation does not install them.

```powershell
coj prepare-worktree
```

Preparation retains build outputs, synchronizes submodules, generates presets/code, and disables
Live Coding in existing editor settings. It does not build or run the complete test suite.
Use `coj presets --check` and the `check-generated-code` target to check generated inputs.

Native and Unreal builds share the pinned MSVC headers, SDK, linker and `/MD` runtime;
simulation uses `_ITERATOR_DEBUG_LEVEL=0`. Unreal game/editor targets use MSVC, while native
implementation files may use newer Clang features. Headers exposed to Unreal must compile
with MSVC. After changing ABI/toolset pins, remove the affected build trees and rebuild native
dependencies before Unreal.

Set `LLVM_ROOT` **in the environment before configuring** to select another LLVM installation.
Otherwise tools come from PATH. CMake captures this selection for child tools/tests;
`-DLLVM_ROOT=...` is not a supported cache option. The installation must contain the required
compiler, archive, inspection and dependency-scanning tools. Missing tools are not auto-installed.
Set `IOJ_WITH_UNREAL=OFF` for standalone native configurations.

### Compiler cache

Clang-cl presets use ccache through CMake's standard C/C++ compiler launchers.
Install ccache on PATH; its default cache directory and user settings are respected.
Use `CCACHE_MAXSIZE=100G` to set a 100 GB limit; `SCCACHE_CACHE_SIZE` does not affect ccache.
Set `CMAKE_C_COMPILER_LAUNCHER` and `CMAKE_CXX_COMPILER_LAUNCHER` to empty strings
to disable caching. Unreal compilation is separately managed by UBT/UBA.

C++ launchers set `sloppiness=pch_defines,time_macros` for PCH caching. Avoid
time-dependent macros in PCHs. Clang-cl uses `/showIncludes` dependencies and
timestamp-free PCHs so both PCH creation and consuming objects can be cached.
The launchers use `compiler_check=content` to distinguish compiler binaries even when
their paths or reported versions match. CMake watches the Clang executable and includes
its content hash in PCH creation flags, so replacing it triggers reconfiguration and
rebuilds retained PCHs automatically. Cache settings alone cannot invalidate an existing
PCH that the build system considers up to date.

Unreal CMake builds enable UBA result caching by default (`IOJ_ENABLE_UBA_CACHE=ON`).
Before invoking UBT, they start the engine's `UbaCacheService.exe` in the background
if no service is listening on `127.0.0.1:1347`. No manual launch or Windows service
installation is needed; the next build restarts it after a reboot. UBT receives
cache read/write settings only in its build environment, preserving XML configuration.
The cache uses Epic's default `%ProgramData%\Epic\UbaCacheService` directory and a
100 GB limit (`IOJ_UBA_CACHE_CAPACITY_GB`). Capacity changes apply on the next service
start. Configure with `-DIOJ_ENABLE_UBA_CACHE=OFF` to stop using this integration;
this does not stop the shared background process or delete cached data.

### Compiler build profiling

The editor target uses unity builds with a 64 KiB source-size target and an eight-file
game-module threshold. Adaptive unity and private PCHs remain enabled. Native unity
presets use batches of eight source files.

Worker limits are machine-specific. On the Ryzen 9 9950X3D / 128 GB development machine,
`Saved/UnrealBuildTool/BuildConfiguration.xml` uses the following section inside its
`Configuration` element to allow 32 actions while retaining Unreal's memory limits:

```xml
<ParallelExecutor>
  <ProcessorCountMultiplier>2.0</ProcessorCountMultiplier>
</ParallelExecutor>
```

This ignored local setting must be applied separately in a new worktree. The measured
clean Unreal executor time fell from 125.64 s (non-unity, 16 workers) to 46.37 s
(unity, 32 workers), with caches bypassed; these are individual runs.

Use `coj profile-build native` for native build timings and ClangBuildAnalyzer output,
or `coj profile-build unreal` for a clean DebugGame editor capture including detailed
UBA tracing. Results remain under `.local/build-profiles/`; temporary Unreal profiling
configuration is restored after success or failure. See the
[profiling workflow](../tools/rust/crates/coj/docs/usage.md#build-profiling) for profile
commands, cleanup and artifact details.

Configure with `-DIOJ_ENABLE_BUILD_PROFILING=ON` to enable compiler timing on targets
using `sandbox::native_defaults`. Other targets can link `ioj::build_profiling`
privately, or propagate it through their own interface libraries. The option defaults
to `OFF` and is separate from `IOJ_ENABLE_PROFILING`, which controls runtime Tracy
instrumentation. It does not change Unreal's UBT compiler settings.

Clang and clang-cl emit `-ftime-trace` JSON beside object files. MSVC emits front-end
and back-end timing in the build output using `/Bt+`; it does not produce Clang JSON.
Other compilers receive no additional flags.

For a fresh native capture, disable compiler launchers and rebuild the relevant targets:

```powershell
cmake --preset native -DIOJ_ENABLE_BUILD_PROFILING=ON -DCMAKE_C_COMPILER_LAUNCHER= -DCMAKE_CXX_COMPILER_LAUNCHER=
cmake --build --preset native --target native-core --clean-first
ClangBuildAnalyzer.exe --all out/build/native .local/native-build-profile.bin
ClangBuildAnalyzer.exe --analyze .local/native-build-profile.bin
```

`ClangBuildAnalyzer.exe` is available on PATH on the development machine. Use a fresh
trace directory, or its `--start` / `--stop` capture workflow, to exclude stale traces.
Reconfigure with `cmake --preset native -DIOJ_ENABLE_BUILD_PROFILING=OFF` afterward;
the preset also restores its compiler launchers. A cache hit is not a fresh compiler
measurement.

## Optional Unreal experiments

Normal editor builds exclude `SandboxExperiments`. Enable that plugin in
`Sandbox.uproject` and rebuild when working on UI/shader showcases, their smoke
tests, or `generate-slate-dsl-smoke-asset`. Module names are retained; showcase assets
are mounted only when the plugin is enabled, with redirects for their old paths.
The experiments-only `SandboxShaders.MaterialCompilation` CTest entry is registered
only when this plugin is explicitly enabled; reconfigure CMake after changing it.
The mesh-generation lab remains enabled because editor asset-generation commands
use it. `SandboxISMCLab` is a separate, disabled-by-default plugin; enable it and
rebuild before using ISMC demo maps or `coj benchmark sandbox-ismc`. The production
`SandboxISMC` renderer and its tests remain enabled.

When updating a workspace built before this module move, remove the generated
`UnrealEditor*.modules` manifests under `Plugins/SandboxUI/Binaries/Win64`,
`Plugins/SandboxShaders/Binaries/Win64` and `Plugins/SandboxISMC/Binaries/Win64`,
then rebuild. Stale entries can otherwise
override the experiments plugin's new binary locations.

The obsolete learning actors, example widgets, editor customizations and their
unused learning/prototype assets have been removed. SpaceGame content is retained.

## Native iteration

### Build-time generators

Build-time generation and checks use a revision-local Release build of `lispb` and
its compiler libraries under `<build-dir>/host-tools/lispb`. This is independent of
the game/library configuration. The generator rebuilds incrementally when its inputs
change; `cmake --build --preset native --target lispb` builds it explicitly.
For generator debugging, configure with `-DIOJ_HOST_TOOLS_CONFIGURATION=Debug`;
restore `Release` afterward. The normal game and compiler-library test targets keep
their preset's configuration. Checks still verify all generated output on each run.

Build affected targets and select tests by subsystem:

```powershell
cmake --build --preset native --target native-simulation-tests
ctest --test-dir out/build/native -L '^native-simulation$' -LE 'soak|compile-contract' --output-on-failure
```

`-L` takes a regular expression; multiple `-L` arguments must all match. `-LE` excludes matching
labels, and `-N` previews selection. The `native` label covers product/library validation;
`developer-tool` covers standalone tools. Avoid the broad `unit` label for a native-only run.

| Scope | Workflow preset |
|---|---|
| Ordinary simulation tests | `native-simulation-tests` |
| Simulation tests and final soak | `native-simulation-full-tests` |
| Simulation soak alone | `native-simulation-soak-tests` |
| All native tests, including compile contracts and soak | `native-tests` |
| Standalone developer tools | `tool-tests` |
| Build a broad baseline without executing tests | `task-start` |

Run a workflow with `cmake --workflow --preset <name>`. Use focused builds/tests during
implementation, then applicable expensive gates once on the final candidate. Run tool tests
only for affected tools or shared tool infrastructure. Rust CTest entries invoke
`cargo test --locked` for their package; `rust-tool-tests-build` prebuilds them.
Build/registration changes also require the applicable `cmake` infrastructure tests.

Clang-tidy is opt-in: see [clang-tidy](clang-tidy.md) for setup, scopes and profiling.
ASAN is also opt-in: `cmake --workflow --preset win-x64-clangcl-debug-asan`.
It requires compiler-rt's Windows DLL, import library and runtime thunk; see
[LLVM setup](../tools/llvm/README.md). `IOJ_ASAN_WORKAROUND_LLVM_215376=ON` disables
stack-use-after-return instrumentation because of an upstream Windows exception bug;
`ASAN_OPTIONS` cannot restore omitted instrumentation. When testing a newer compiler,
configure that preset with `-DIOJ_ASAN_WORKAROUND_LLVM_215376=OFF` and rebuild.

## Unreal integration

Build Unreal when engine-facing code, UI, reflection, assets or editor behavior changes.
For a focused integration pass:

```powershell
cmake --preset debug-game
cmake --build --preset debug-game --target editor
ctest --test-dir out/build/debug-game -R '^Sandbox.UnitTests$' --output-on-failure
```

`debug-game-unit-tests` combines the editor build and unit tests. `debug-game-tests` is the
game/native integration workflow; `debug-game-full-tests` additionally includes standalone
developer tools and is for explicitly requested broad validation. `debug-game` prepares an
editor-ready build without running tests. Use `development` for Development configuration.

Unreal modules using `CppStandard.Latest` cannot consume the installed engine's shared
PCHs. Engine-facing modules declare `PrivatePCHHeaderFile` in their `.Build.cs` files
to retain PCHs with that standard. Keep these headers limited to stable engine headers
commonly used by the module; avoid adding frequently edited gameplay headers. The small
`SandboxCore`, `SpaceGameSimulation`, `USFLoader` and `SandboxMaterialExprs` modules
remain without PCHs because their measured compilation work is small.
Keep `ShortenSizeTToIntWarningLevel` at Unreal's default: explicit `/we4267` overrides
are restored when MSVC loads a PCH, bypassing the engine header's warning suppression
and producing errors in engine headers.

Launch with `coj editor` (optionally `--wait-for-debugger`), or pass `--build-dir` for another
configured tree. To prepare Visual Studio debugging without a full build:

```powershell
cmake --preset debug-game
cmake --build --preset generate-worktree-code-debug-game
cmake --build --preset debug-game --target unreal-generated-inputs unreal-native-dependencies
coj unreal project-files --build-dir out/build/debug-game
```

Use `DebugGame Editor | Win64` or `Development Editor | Win64` in the generated solution.
`coj unreal project-files` can also use `UE_ROOT` directly without CMake configuration.
Live Coding is disabled. If tests report missing/stale modules, rebuild the matching editor
target; never repair UBT receipts, manifests or BuildIds manually.

The explicit configuration-transition regression builds and smoke-tests DebugGame,
Development, then DebugGame again:

```powershell
pwsh -NoProfile -File PowerShell/tests/TestUnrealEditorConfigurationTransition.ps1
```

## Packaging and assets

```powershell
cmake --workflow --preset development-staged-game
cmake --workflow --preset development-package
pwsh -NoProfile -File PowerShell/PackageGame.ps1
```

The PowerShell command builds the full Shipping package. `coj run-staged` launches an existing
package; pass `--build-dir out/build/shipping` for Shipping. See [PowerShell](../PowerShell/README.md).

Use `coj unreal --help` for asset commands, including scripted levels, meshes, materials,
resaving and audio import. They build their editor/material prerequisites and update authored
content. They default to `out/build/debug-game`; `--build-dir` selects another configuration.
Audio import uses `BEE_AUDIO_ROOT` pointing to the `sci-fi_ds_2220mb` pack and leaves existing
assets intact when the pack is unavailable. Format with `coj format --changed`.

## Coordination and integration

Follow the [jobs-board protocol](../tools/jobserver/README.md) for heavy work: request, check
until Ready, start immediately before execution, and end on success or failure. Use exclusive
access for benchmarks and builds that write shared engine output. Do not overlap editors,
Visual Studio, Live Coding or direct UBT with these builds. Normal builds omit
`-NoEngineChanges` because project runtime dependencies are staged into engine output.

After validation and explicit user authorization, `coj integrate` rebases and promotes the
candidate; it does not rerun gates. Resolve rebase conflicts with `coj git rebase dev`, validate
the result, and retry. See [benchmarks](benchmarks.md) and [profiling](profiling.md) for performance work.
