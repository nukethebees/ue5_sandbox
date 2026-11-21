# Space combat sandbox

An Unreal Engine 5.8 space combat game with a C++ simulation that also runs outside the editor.

## Setup

Prerequisites:

- Windows with Developer Mode enabled and Unreal Engine 5.8 installed
- Visual Studio C++ tools with MSVC 14.50.35717 and Windows SDK 10.0.22621.0 ([project pins](Config/DefaultEngine.ini))
- Clang 24 with `clang-cl` and `clang-format`, plus DirectX Shader Compiler (`dxc`)
- CMake 4.4.2+, Ninja, ccache, Git, Python 3, and PowerShell 7
- Rust via rustup, which selects the project's pinned toolchain

Set these before running commands from the repository root:

- `UE_ROOT` points to the Unreal Engine installation.
- `IOJ_ROOT` points to an absolute directory for shared tools and temporary files, such as `C:\dev\ioj`.
- Add the command-line tools and `%IOJ_ROOT%\tools\bin` to `PATH`.
- Optionally set `LLVM_ROOT` to select a specific LLVM installation.

Install the repository CLI once per machine:

```powershell
pwsh -NoProfile -File PowerShell/InstallCoj.ps1
```

## Build and play

Run the first command once per fresh worktree session. It syncs submodules and generates presets and code.

```powershell
coj prepare-worktree
cmake --workflow --preset debug-game
coj editor
```

Press **Play** in the editor to start the game.

## Other builds

- **Standalone native libraries** — `cmake --workflow --preset native-lean`
- **Visual Studio project files** — `coj unreal project-files --build-dir out/build/debug-game`. Select `DebugGame Editor` and `Win64`.
- **Stage and launch the game** — `cmake --workflow --preset development-staged-game`, then `coj run-staged`
- **Package the game** — `cmake --workflow --preset development-package`

Editor builds, cooking, and automation share Unreal's local Zen derived data cache. Keep it between builds.

## Tests

- **Simulation suite** — `cmake --workflow --preset native-simulation-tests`
- **Full native suite**, including soak and compile contracts — `cmake --workflow --preset native-tests`
- **Developer tools** — `cmake --workflow --preset tool-tests`
- **Unreal unit tests** — `cmake --workflow --preset debug-game-unit-tests`
- **All editor automation** with an existing editor build — `ctest --preset debug-game-all-unreal-tests`

For a focused simulation run:

```powershell
cmake --build --preset native --target native-simulation-tests
ctest --test-dir out/build/native -L '^native-simulation$' -LE 'soak|compile-contract' --output-on-failure
```

## Guides

- [Author a mission](LevelScripts/README.md)
- [Regenerate C++ or assets](native/lispb/README.md)
- [Open a shader or UI showcase](Plugins/SandboxExperiments/README.md)
- [Compare revisions or capture a trace](docs/benchmarks.md)
- [Run the developer tools](tools/README.md)

[Native libraries](native/README.md) · [Unreal plugins](Plugins/README.md) · [Material sources](FabAssets.md)

[Build ownership](cmake/ARCHITECTURE.md) · [CMake style](cmake/STYLE.md) · [Clang-tidy](docs/clang-tidy.md)
