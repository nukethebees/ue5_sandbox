# Build and test

The root CMake project is the supported entry point for Unreal builds, editor tests, commandlets,
and native targets. Do not invoke UBT, `RunUBT.bat`, or `Build.bat` directly.

## First-time or worktree setup

Install CMake 4.4.2 or newer and Ninja, set `UE_ROOT` to the Unreal Engine installation, then
initialize the worktree:

```powershell
. .\dev.ps1
csetup
```

`csetup` synchronizes submodules, regenerates native presets, installs the per-user jobserver when
needed, and prepares the DebugGame and Development worktrees. Pass `debug-game` or `development`
to prepare only that configuration. See the [PowerShell guide](../PowerShell/README.md) for the
other session commands.

Alternatively, set `UE_ROOT` in the ignored `CMakeUserPresets.json` using a local configure preset
that inherits from `development`. CMake builds pinned native dependencies from source; no package
manager configuration is needed.

## Everyday workflows

```powershell
# Prepare and build Editor-ready configurations.
cplay debug-game

# Configure and build directly through CMake.
cmake --workflow --preset debug-game

# Run all DebugGame test suites, or the unit-labelled suites only.
cmake --workflow --preset debug-game-tests
cmake --workflow --preset debug-game-unit-tests

# Rerun level tests after a build.
ctest --preset debug-game-level-tests
```

Use `cmake --build --preset debug-game --target run-editor` to build and launch the Editor, or
`run-editor-debug` to break at startup for an attached debugger. Regenerate Visual Studio project
files with `cprojectfiles` after module, plugin, target, or build-rule changes.

Live Coding is disabled by tracked project configuration and setup normalizes the local Editor
setting. Use the generated `Sandbox` solution with `DebugGame Editor | Win64` or
`Development Editor | Win64` for Visual Studio debugging.

`csetup` creates the worktree dependencies required by game and editor targets, including native
memory, image, material-generation, mesh-generation, CPU-feature, and generated-code artifacts.
DebugGame setup can import the shared audio assets when `BEE_AUDIO_ROOT` points to the
`sci-fi_ds_2220mb` pack. Run `cmake --workflow --preset import-game-audio` to invoke that import
separately; it succeeds without replacing assets when the source pack is unavailable.

## Packaging and asset maintenance

Use the CMake workflows for iterative staged and Development packages:

```powershell
cmake --workflow --preset development-staged-game
cmake --workflow --preset development-package
```

Use the full Shipping package path with:

```powershell
pwsh -NoProfile -File PowerShell/PackageGame.ps1
```

The `resave-assets` workflow modifies assets, so ensure intended files are writable before running
it.

CTest discovers Catch2 unit tests, standalone native GoogleTests, and the `Sandbox.LevelTests`
Unreal Automation group. Low-level tests use the `unit` label and level tests use the `level` label.
If Unreal tests report missing project plugin modules, build the `editor` target through the
`debug-game` preset to repair stale Editor-module BuildIds before rerunning them.

## Coordination

CMake-managed Unreal builds take the repository's canonical engine gate; managed editor processes,
tests, and commandlets hold a compatible shared claim for their lifetime. The per-user jobserver
also coordinates costly work across worktrees. Load `dev.ps1` and run `get-jobserver-state` to
inspect it. Do not overlap manually launched Editors, Visual Studio builds, Live Coding, or direct
UBT work with a managed Unreal build.

See [the CMake guide](../cmake/README.md) for preset structure and [the native guide](../native/README.md)
for standalone-only workflows.
