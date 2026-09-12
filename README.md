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
| SandboxNative | Editor/engine independent code |
| SandboxNativeTests | Tests for `SandboxNative` |
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
are `croot`, `cwt <name>`, `cwb [branch]`, `cplugin <name>`, `ctests`, `csetup`, and
`cprojectfiles`; `dev-help` repeats the help after loading. Run `cwb` without a branch to list
checked-out branches and their worktree directories.

## Command-line builds

CMake 4.3 or newer and Ninja on `PATH` provide a small command-line wrapper around UnrealBuildTool (UBT). It does not
compile Unreal modules itself; `.Target.cs`, `.Build.cs`, and UBT remain authoritative.

CMake invokes the source engine's `RunUBT.bat` and serializes Unreal builds that share the same engine checkout. Manual
Visual Studio builds, Live Coding, and other Unreal builds launched outside CMake do not participate in that lock. Do
not run one of those external builds at the same time as a CMake Unreal build.

CMake-coordinated compiles, links, Unreal commandlets, and editor tests also participate in a
per-user machine activity gate shared by every worktree. Requests are ordered FIFO. Standard work
may overlap unless an older benchmark is waiting or running. A benchmark waits for every older
request and then runs exclusively, while later work waits behind it. Inspect the gate with
`get-machine-activity-state` after loading `dev.ps1`, or build the `machine-activity-status` target.
The transparent ticket files live under `%TEMP%\SandboxUnrealBuild\activity\v1`.

### vcpkg dependencies

The CMake presets use the root `vcpkg.json` manifest for native dependencies. Install a standalone
vcpkg copy outside Visual Studio (for example, `C:\\dev\\vcpkg`) and set the persistent user
environment variable `VCPKG_ROOT` to that directory. Restart terminals, Visual Studio, and Codex
after changing it.

Verify the selected installation explicitly:

```powershell
& "$env:VCPKG_ROOT\\vcpkg.exe" version
```

Use that explicit form rather than bare `vcpkg` if a Visual Studio Developer shell places its embedded
vcpkg copy earlier on `PATH`. To make bare `vcpkg` reliable too, put `C:\\dev\\vcpkg` before the Visual
Studio vcpkg directory in your user `PATH`.

`vcpkg_installed` is generated per worktree and ignored by Git. CMake installs the required manifest
dependencies when configuring a preset. If a worktree retains a CMake cache
from a previous vcpkg location, delete only that worktree's `out/build/<preset>` directory and rerun
the workflow.

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

### Preparing a worktree

After setting `VCPKG_ROOT` and `UE_ROOT`, load the development commands and prepare a new or
reset worktree with:

```powershell
. .\dev.ps1
csetup
```

By default, `csetup` prepares DebugGame and Development. Pass `debug-game` or `development` to
prepare only one configuration, for example `csetup debug-game`. Each variant configures its build
tree and installs per-worktree vcpkg dependencies, builds every first-party
non-Unreal dependency consumed by the Unreal project, and generates Visual Studio project files.
The Development variant does not build an Unreal target. DebugGame also performs the shared audio
import described below. Both are safe to rerun after switching branches or changing project
definitions.

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
| Generated C++, kernel, and Slate checks | Game and Editor source compilation |
| Compiled UI-glow material IR | Editor material generation |

Configuration also provisions the manifest's `cpu-features` and `gtest` packages under the
worktree's ignored `vcpkg_installed` directory. SandboxCore builds its private, symbol-prefixed
mimalloc implementation from the vendored source in this repository.

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

CTest discovers individual tests from the `SandboxCoreTests` and `SandboxNativeTests`
executables at test time and runs Unreal Automation Test groups through the configured
Editor. Low-level tests use the `unit` label and Unreal level tests use the `level`
label. To rerun the level group without rebuilding:

```powershell
cd out/build/debug-game
ctest -R Sandbox.LevelTests --output-on-failure
```

Alternatively, use `ctest --preset debug-game-level-tests` from the project root.
Use `cmake --workflow --preset debug-game-unit-tests` for the unit-labelled suites only.
