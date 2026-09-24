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

`csetup` synchronizes submodules, regenerates presets, installs the per-user jobserver when
needed, and prepares the DebugGame and Development worktrees. Its CMake workflows build their
configuration-local C# host-tool dependencies on demand. Use `csetup native` when only the native
toolchain is needed; it avoids Unreal worktree preparation. See the
[PowerShell guide](../PowerShell/README.md) for the other session commands.

Alternatively, set `UE_ROOT` in the ignored `CMakeUserPresets.json` using a local configure preset
that inherits from `development`. CMake builds pinned native dependencies from source; no package
manager configuration is needed.

Windows native builds and Unreal share the `CompilerVersion` and `WindowsSDKVersion` pins in
`Config/DefaultEngine.ini`. Install those MSVC x64 build tools and Windows SDK versions through
Visual Studio Installer. Both CMake toolchains use the pinned headers and libraries, including
when run from a developer prompt for a different toolset. After changing either pin, remove the
affected `out/build/<preset>` directories and rebuild the native libraries before building Unreal.

Project-owned build options and compile definitions use the `IOJ_` prefix. CMake passes
`IOJ_NATIVE_TOOLCHAIN` to UnrealBuildTools, project-file generation, and UAT; Unreal module rules
read that same environment variable. Set `IOJ_WITH_UNREAL=OFF` for standalone native builds.

## Development validation

Use the cheapest tier that validates the changed boundary. Native code is the normal inner loop;
Unreal is an integration boundary. The final integration planner selects the relevant gate from
the changed component graph; the normal DebugGame game/native workflow is reserved for
Unreal-facing or cross-cutting candidates.

### Native inner loop

```powershell
# Configure the default native clang-cl Debug + unity build. Unreal is disabled.
cmake --preset native

# Build only the changed native test target, then run its focused tests.
cmake --build --preset native --target native-simulation-tests
ctest --preset native-simulation-tests

# Run the complete first-party native suite.
cmake --workflow --preset native-tests
```

`native-core-tests` and `native-simulation-tests` are focused workflows. `native-tests` builds and
runs all first-party tests under `native/`; it does not configure UBT or launch UnrealEditor. The
PowerShell shortcut `cbuild` defaults to `native-tests`.

Native mimalloc targets build their configuration-local `NativeBinaryTools` host dependency on
demand. CMake-owned tools likewise rebuild automatically when their sources change; no CMake
workflow requires a manual `ctools` preflight.

### Standalone developer tools

```powershell
cmake --workflow --preset tool-tests
```

Run this only when the current change can affect a standalone developer tool: the tool or its
tests, a directly consumed interface/protocol/file format/configuration, shared tool/build
infrastructure, or an active tool diagnosis. Normal native, game, runtime, DebugGame unit, and
DebugGame integration validation deliberately exclude this suite.

Do not rebuild Unreal merely because a native implementation has a thin Unreal adapter. Settle the
native behavior with the smallest target and test subset first.

### Focused Unreal integration

Use Unreal validation once an Unreal-facing boundary needs checking: module/build definitions,
reflection or UObject lifetime, engine adapters/APIs, UI, assets, or editor behavior.

```powershell
# Build the smallest broad cross-layer unit prerequisite, then run unit taxonomy tests.
cmake --workflow --preset debug-game-unit-tests

# Prepare and build an Editor-ready configuration when interactive validation is needed.
cplay debug-game
```

`debug-game-unit-tests` includes an Editor build and is not a native inner-loop command. For a
single integration concern, prefer a focused CMake target and CTest name/label filter over this
workflow.

### Merge-ready integration

After implementation is complete and the feature branch has been rebased onto current `dev`, run:

```powershell
cmake --workflow --preset debug-game-tests
```

This is the normal DebugGame game/native integration gate. If it finds one native failure, return to that
target's focused native build/test loop, then rerun this gate once on the final HEAD.

Use `cmake --workflow --preset debug-game-full-tests` only for explicitly requested broad
validation; it also includes standalone developer-tool tests.

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

CTest labels separate taxonomy from integration cost: native tests carry `native` plus their
existing unit/subsystem labels, while Unreal Automation tests retain their semantic labels.
Use the native presets rather than the generic `unit` label for the everyday native path. If Unreal
tests report missing or stale project/plugin modules, build the `editor` target through the matching
CMake preset before rerunning them. UBT owns target receipts, module manifests, and BuildIds; do not
edit or synchronize that metadata manually.

Run the focused configuration transition regression with:

```powershell
pwsh -NoProfile -File PowerShell/TestUnrealEditorConfigurationTransition.ps1
```

It takes one exclusive engine lease, then builds and smoke-tests `SandboxEditor` as DebugGame,
Development, then DebugGame again without asserting any particular BuildId value. Holding the
parent lease prevents another worktree from invalidating one step's UBT metadata before its smoke
reader starts.

## Coordination

CMake-managed Unreal builds take the repository's canonical engine gate exclusively. Unattended
editor tests and commandlets hold a compatible shared claim for their lifetime; interactive managed
editor launches hold it exclusively because Unreal can prompt to compile missing modules during
startup. The per-user jobserver also coordinates costly work across worktrees. Load `dev.ps1` and
run `get-jobserver-state` to inspect it. Do not overlap manually launched Editors, Visual Studio
builds, Live Coding, or direct UBT work with a managed Unreal build.

Normal project builds intentionally do not pass `-NoEngineChanges`. Project-owned runtime
dependencies such as `SandboxTracyClient.dll` are staged into the Editor target's engine output
directory, which that option treats as an engine change. Correctness instead comes from routing the
entire UBT process tree through the exclusive canonical engine claim.

See [the CMake guide](../cmake/README.md) for preset structure and [the native guide](../native/README.md)
for standalone-only workflows. Use [Benchmarks](benchmarks.md) for exclusive performance
measurements and [Profiling](profiling.md) to capture native level benchmarks with Tracy.
