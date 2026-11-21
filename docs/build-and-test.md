# Build and test

## Setup

Use Windows with Developer Mode, PowerShell 7, Git, Python 3, Rust via rustup, CMake 4.4.2+, Ninja, ccache, Clang 24 and `dxc` on PATH. Install Unreal 5.8 and the MSVC and Windows SDK versions pinned in [engine config](../Config/DefaultEngine.ini).

Set `UE_ROOT` to the engine directory and `IOJ_ROOT` to an absolute shared tools directory. Add `%IOJ_ROOT%\tools\bin` to PATH. If selecting a specific LLVM installation, set `LLVM_ROOT` before configuring.

The maintainer installs `coj` from the repository root with `pwsh -NoProfile -File PowerShell/InstallCoj.ps1`. Run `coj prepare-worktree` once per fresh session to sync submodules and generate presets and code.

## Build and run

| Task | Command |
| --- | --- |
| Build native code | `cmake --workflow --preset native-lean` |
| Build the Unreal editor | `cmake --workflow --preset debug-game` |
| Launch the editor | `coj editor` |
| Generate Visual Studio files | `coj unreal project-files --build-dir out/build/debug-game` |
| Stage and launch the game | `cmake --workflow --preset development-staged-game`, then `coj run-staged` |
| Package the game | `cmake --workflow --preset development-package` |

Use `DebugGame Editor` on `Win64` in Visual Studio. Editor, cooking and automation share Unreal's local Zen derived data cache. Preserve it across builds.

## Test

Build the affected target, then run its CTest label. For a short simulation run:

```powershell
cmake --build --preset native --target native-simulation-tests
ctest --test-dir out/build/native -L '^native-simulation$' -LE 'soak|compile-contract' --output-on-failure
```

Use `cmake --workflow --preset` with `native-simulation-tests` for the simulation suite, `native-tests` for all native tests including soak and compile contracts, `tool-tests` for developer tools, or `debug-game-unit-tests` for Unreal unit tests. With an existing editor build, `ctest --preset debug-game-all-unreal-tests` runs all project editor automation.

[Code generation](../native/lispb/README.md) · [Clang-tidy](clang-tidy.md) · [CMake style](../cmake/STYLE.md) · [Build ownership](../cmake/ARCHITECTURE.md)
