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

Generated C++ files are defined by the JSON schemas in `Codegen/manifests`. The standalone
C++ generator is built with CMake and regenerates the files from the repository root:

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
are `croot`, `cwt <name>`, `cwb [branch]`, `cplugin <name>`, and `ctests`; `dev-help`
repeats the help after loading. Run `cwb` without a branch to list checked-out branches
and their worktree directories.

## Command-line builds

CMake 4.3 or newer and Ninja on `PATH` provide a small command-line wrapper around UnrealBuildTool (UBT). It does not
compile Unreal modules itself; `.Target.cs`, `.Build.cs`, and UBT remain authoritative.

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
