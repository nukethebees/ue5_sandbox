# Sandbox Unreal Engine Project

This repository is a Sandbox for me to learn UE5.
It contains two games mashed into the one repo:

* An abandoned attempt at an immersive sim style shooter
* A Starfox/X-Wing style flight shooter

## Custom Plugins

The project utilizes a custom plugin to extend the engine's capabilities:

*   **USF Loader**: Located in `Plugins/USFLoader/`, it allows you to inject `.usf/.ush` files into materials to call HLSL functions within custom material nodes.

## Project Dimensions (cm)

| Item | Width | Height | Depth | 
| --- | --- | --- | --- |
| Outer Wall | - | 300 | 30 |
| Inner Wall | - | 300 | 20 |
| Floor | - | - | 20 |
| Door | 100 | 220 | -

## Modules

| Name | Purpose |
| --- | --- | 
| Sandbox | Main game code |
| SandboxEditor | Editor code |
| SandboxNative | Unreal adapters for the standalone native libraries |
| SandboxTests | Tests for `Sandbox` |

## Code generation

The `lispb` native tool generates C++ schemas, Slate, kernels, and compiled material IR from
`.lispb` inputs. Named targets and groups are defined in `lispb/project.lispb`; the general C++
schemas live under `lispb/schema`. Regenerate committed files from the repository root with:

```bash
cmake --workflow --preset generate-code
```

To verify that committed generated files are current without writing them:

```bash
cmake --build --preset codegen --target check-generated-code
```

## Development commands

Load project navigation commands into the current PowerShell session by dot-sourcing
the root script:

```powershell
. .\dev.ps1
```

The leading dot matters: it loads the project's functions into the current session.
Use `.\dev.ps1 --help` to discover commands without loading them. The initial commands
are `croot`, `cwt <name>`, `cwb [branch]`, `cplugin <name>`, `ctests`, `csetup`, `cplay`, and
`cprojectfiles`; `dev-help` repeats the help after loading. Run `cwb` without a branch to list
checked-out branches and their worktree directories.

## Command-line builds

CMake 4.4.2 or newer and Ninja on `PATH` provide a small command-line wrapper around UnrealBuildTool (UBT). It does not
compile Unreal modules itself; `.Target.cs`, `.Build.cs`, and UBT remain authoritative.

CMake invokes the source engine's `RunUBT.bat` and asks the per-user jobserver to serialize Unreal
builds that share the same engine checkout. Manual Visual Studio builds, Live Coding, and other
Unreal builds launched outside CMake do not participate in that resource claim. Do not run one of
those external builds at the same time as a CMake Unreal build.

CMake-coordinated compiles, links, Unreal commandlets, and editor tests also participate in a
canonical per-user jobserver shared by every worktree. Requests are ordered fairly by conflicting
resource claims. Standard work may overlap unless an older benchmark is waiting or running. A
benchmark waits for older machine work to drain and then runs exclusively, while later conflicting
work waits behind it. Use `get-jobserver-state` after loading `dev.ps1`, or build the
`jobserver-status` target.

### Native dependencies

Native dependencies are pinned Git submodules under `native/third_party`, including GoogleTest,
cpu_features, Google Benchmark, CLI11, and nlohmann/json. Initialize them after cloning or creating
a worktree:

```powershell
git submodule update --init native/third_party/googletest native/third_party/cpu_features `
  native/third_party/benchmark native/third_party/cli11 native/third_party/nlohmann_json
```

CMake builds these dependencies from source. No package manager installation or environment
variable is required.

This initial wrapper supports Windows builds. Set `UE_ROOT` to the root of a
usable Unreal Engine installation, either in the environment or in an untracked
`CMakeUserPresets.json` that inherits from `development`:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "local-development",
      "inherits": "development",
      "cacheVariables": {
        "UE_ROOT": "<path-to-unreal-engine>"
      }
    }
  ]
}
```

With `UE_ROOT` available, configure and build the Development Editor and low-level test targets:

```bash
cmake --preset development
cmake --build --preset development
```

The `debug`, `debug-game`, `development`, `shipping`, and `test` presets select their
matching Unreal configuration and build `dev-core` (`editor`, `core-tests`, and
`native-tests`). Each also has a workflow preset, for example `cmake --workflow --preset development`, that
configures and builds it in one command. The `game` target remains available through a build preset, for example
`cmake --build --preset development --target game`.

The existing presets compile the standalone native libraries with clang-cl. Append `-msvc` to select
MSVC instead, for example `debug-game-msvc` or `development-msvc`. UnrealBuildTool remains
authoritative for Unreal compilation; the selected native compiler is passed to the Unreal module
rules so they link the matching libraries. Each compiler uses a separate build tree and native
artifact directory.

### Preset organisation

The root `CMakePresets.json` includes category files under `cmake/presets/`, using preset
schema version 9:

- `base.json`: generated shared configure, platform/toolchain and test defaults.
- `native.json`: generated native configuration matrix and specialized codegen workflows.
- `unreal.json`: Unreal configurations, builds, automation tests and development tooling.
- `native-benchmarks.json`: generated native kernel and SOA benchmark presets.
- `unreal-benchmarks.json`: Unreal-backed benchmark presets.

Each category includes its prerequisites; shared definitions are not duplicated. Local overrides
still belong in the root, Git-ignored `CMakeUserPresets.json`.
Regenerate the generated files with `python cmake/presets/generate.py`; `csetup` does this before
its normal worktree setup. Use `python cmake/presets/generate.py --check` to verify that they are
current without changing them.

### Native-only development

All native libraries and tools are configured together through `native/CMakeLists.txt`.
Native preset names encode platform, architecture, compiler, configuration and enabled optional
features. Names without `-unity` are intended for Visual Studio editing; select the corresponding
`-unity` preset for faster full builds. These configure presets set
`SANDBOX_WITH_UNREAL=OFF`, so no Unreal installation or `UE_ROOT` is required.
The Unreal configure presets enable that option. Standalone native libraries remain in their
respective build trees; only Unreal-enabled configurations publish libraries under `Binaries/`.

Native tests and tools always link a static Tracy client. Unreal-enabled configurations also
compile `native-simulation-unreal` against a shared Tracy client, publishing the simulation
archive and client DLL under `Binaries/Native/`. The two simulation targets use the same sources;
standalone executables need no Tracy DLL staging or additional `PATH` entries.

```powershell
cmake --workflow --preset win-x64-clangcl-debug
cmake --workflow --preset win-x64-clangcl-release-unity
cmake --workflow --preset win-x64-msvc-debug
cmake --workflow --preset win-x64-msvc-release-unity
cmake --workflow --preset win-x64-clangcl-debug-asan
cmake --workflow --preset win-x64-clangcl-release-asan-unity

cmake --build --preset codegen
ctest --preset codegen-tests
```

The aggregate workflows use compiler- and feature-specific build trees. The specialized `codegen`
and `generate-code` presets use `win-x64-clangcl-debug-unity`; native benchmark presets remain
opt-in and use `win-x64-clangcl-release-unity`. To build an individual target, configure the
desired native preset and pass the target explicitly to
`cmake --build out/build/<preset> --target <target>`.

The Windows clang-cl ASAN test presets exclude 15 code-generation error-path tests that deliberately
throw and inspect C++ exceptions. With LLVM 21's Windows ASAN runtime those tests terminate with an
access violation while accessing the caught exception, although they pass in the corresponding
non-ASAN clang-cl and MSVC presets. The exclusions are listed explicitly in
`cmake/presets/features.py`; all other native tests remain enabled in ASAN workflows.

Simulation logic and worldless combat scenarios run in native GoogleTest tests under
`native/simulation/tests/`. Unreal retains asset/configuration conversion, collision harvesting,
HUD/presentation and memory-bootstrap integration coverage. `dev-core` also builds the native
simulation tests, and the normal unit/all CTest presets include them.

The native simulation fixture captures the converted feature-test level configuration, mesh
bounds, socket transforms and player defaults. To refresh the capture, build `editor`, then run
`ctest --test-dir out/build/debug-game -R '^Sandbox.ExportSimulationFixture$' --output-on-failure`.
This writes `.local/simulation_fixture.cpp`; review it against
`native/simulation/tests/support/simulation_fixture.cpp` before updating the checked-in snapshot.
Native tests do not load Unreal assets or regenerate this fixture automatically.

### Fighter simulation benchmark

The headless fighter scheduling benchmark uses the existing `native-simulation-benchmark` runner
with `LevelScripts/FighterSchedulingBenchmark.scm`. It is intended to compare steady-state fighter
simulation costs, including awareness scans, target selection, navigation, movement, firing/LOS and
normal task processing, before and after scheduling changes.

Run the default 2,000- and 4,000-fighter cases with:

```powershell
pwsh -NoProfile -File Scripts/run-fighter-simulation-benchmark.ps1
```

Run the full initial scaling matrix with:

```powershell
pwsh -NoProfile -File Scripts/run-fighter-simulation-benchmark.ps1 `
    -FighterCaps 1000,2000,4000,8000,16000
```

The matrix script delegates each case to the shared `Scripts/run-native-simulation-benchmark.ps1`
runner. That runner builds the release benchmark preset, acquires exclusive benchmark and machine
access through the jobserver, and invokes `native-simulation-benchmark`. The scenario uses the
production fighter population cap and enough capital ships to saturate it. Measurement begins only
after exact saturation and a post-saturation warm-up (five seconds by default), then records ten
seconds (600 ticks) by default. Fighter laser damage is disabled and ship health is raised to keep
the population stable while leaving normal fighter behavior and collision work enabled.

Each run verifies a constant fighter population, attacking task state, active firing, no replacement
spawns during measurement and no frame-memory overflow. Results are written beneath
`.local/benchmarks/fighter-simulation/<timestamp>/` as detailed JSON and a summary CSV containing
the configured and actual fighter counts, measured ticks, elapsed time, mean/median/p95/p99 tick
times, ticks per second and realtime factor. Use `-Seconds`, `-WarmupSeconds`,
`-SaturationTimeoutSeconds` and `-OutputDirectory` to override the defaults; use `-SkipBuild` only
when the benchmark binary is already current.

The benchmark feature presets remain separate because they enable additional dependencies
or report tooling. Native configurations retain the existing Unreal-compatible CRT/ABI
settings; `UE_CONFIGURATION` still controls native artifact configuration and some compiler
flags even when Unreal integration is off. The general native presets use `DebugGame`
for that setting and select Debug/Release through `CMAKE_BUILD_TYPE`.

Configuring all native targets also requires their configuration tools, including Python,
clang-format, llvm-nm and llvm-readobj, even when building only one library.

### Preparing a worktree

After setting `UE_ROOT`, load the development commands and prepare a new or reset worktree with:

```powershell
. .\dev.ps1
csetup
```

`csetup` synchronizes pinned submodules, configures the build tree, builds dependencies, and
generates project files. It prepares DebugGame and Development by default; pass one or more modes
to limit it, for example `csetup debug-game`. Development setup skips the Unreal target, while
DebugGame imports the shared audio assets. It is safe to rerun after branch or project-definition
changes.

`cplay` runs `csetup` and builds the selected Editor-ready targets; it also defaults to DebugGame
and Development:

```powershell
cplay
cplay debug-game
```

### Launching and debugging the Editor

Live Coding is disabled by the project's tracked Unreal configuration. Worktree setup also
normalizes an existing ignored Editor user setting without changing unrelated preferences, and
generated Visual Studio Editor configurations receive the same explicit override automatically.
Use the generated `Sandbox` project and the `DebugGame Editor | Win64` or
`Development Editor | Win64` configuration for normal Visual Studio F5 debugging.

To build and launch the Editor from the command line with Live Coding disabled:

```powershell
cmake --build --preset debug-game --target run-editor
```

To pause Editor startup until Visual Studio attaches:

```powershell
cmake --build --preset debug-game --target run-editor-debug
```

Attach Visual Studio to `UnrealEditor` and continue from the initial debugger break. Both launch
targets remain attached to the invoking terminal until the Editor exits.

The configuration-specific workflows can also be run directly:

```powershell
cmake --workflow --preset setup-worktree-debug-game
cmake --workflow --preset setup-worktree-development
```

To rebuild only the worktree dependencies for one configured preset:

```powershell
cmake --build --preset debug-game --target worktree-dependencies
```

To regenerate only the Unreal and Visual Studio project files, without building those dependencies:

```powershell
cprojectfiles
```

Pass `development` to use that configured build tree instead, or invoke the underlying target
directly:

```powershell
cmake --build --preset debug-game --target generate-project-files
```

The explicit `worktree-dependencies` build step prepares:

| Dependency | Consumer |
| --- | --- |
| `native-memory` | Game and Editor targets through the `NativeMemory` Unreal module |
| `sandbox-image` | The Editor's `GenLab` module |
| `sandbox-material-gen` | The `SandboxEditor` module |
| `sandbox-mesh-gen` | The Editor's `SbxMeshGenLab` module |
| `cpu_features` | The `CpuFeatures` Unreal module |
| Generated C++, kernel, and Slate checks | Game and Editor source compilation |
| Compiled UI-glow material IR | Editor material generation |

GoogleTest, cpu_features, and the optional Google Benchmark dependency build from the pinned
submodules. SandboxCore builds its private, symbol-prefixed mimalloc implementation from the
vendored source in this repository.

Creating audio `.uasset` files requires an Editor commandlet, so the DebugGame setup imports the
shared audio assets once and may build the project Editor if needed. Set `BEE_AUDIO_ROOT` to the
directory containing the `sci-fi_ds_2220mb` audio pack. The import can also be run separately with:

```powershell
cmake --workflow --preset import-game-audio
```

That workflow builds the DebugGame Editor if needed. When the variable or expected source files are
unavailable, the commandlet succeeds without creating or replacing the generated audio assets.

To resave project assets and fix redirectors, build `editor` and then run the
`ResavePackages` commandlet through:

```bash
cmake --workflow --preset resave-assets
```

This target modifies project assets. It does not automatically check out files from
source control, so affected files must already be writable.

To build and run the CTest suites, including Catch2 low-level unit tests and the
`Sandbox.LevelTests` Unreal Automation Test group:

```bash
cmake --workflow --preset debug-game-tests
```

CTest discovers individual Catch2 tests from `SandboxCoreTests`, GoogleTests from the
standalone native test executables, and Unreal Automation Test groups through the configured
Editor. Low-level tests use the `unit` label and Unreal level tests use the `level` label. To
rerun the level group without rebuilding:

```powershell
cd out/build/debug-game
ctest -R Sandbox.LevelTests --output-on-failure
```

Alternatively, use `ctest --preset debug-game-level-tests` from the project root.
Use `cmake --workflow --preset debug-game-unit-tests` for the unit-labelled suites only.
