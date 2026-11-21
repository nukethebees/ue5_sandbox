# Sandbox Unreal Engine Project

Unreal Engine 5.8 space-combat game with a standalone C++ simulation.

## Prerequisites

- Windows with Developer Mode enabled
- Rust via rustup (the repository pins the toolchain)
- Clang 24, including `clang-cl` and `clang-format`
- Visual Studio C++ tools: MSVC 14.50.35717 and Windows SDK 10.0.22621.0
- CMake 4.4.2+, Ninja, ccache, Python 3, Git, and PowerShell 7
- Unreal Engine 5.8

Set `UE_ROOT` to the engine directory and `IOJ_ROOT` to a shared tools directory
(for example, `C:\dev\ioj`). Add `%IOJ_ROOT%\tools\bin` and the command-line tools to PATH.

## Set up

Install the repository tools and prepare the worktree from the repository root.

```powershell
pwsh -NoProfile -File PowerShell/InstallCoj.ps1
coj prepare-worktree
```

## Build and run

| Task | Command |
| --- | --- |
| Build native code | `cmake --workflow --preset native-lean` |
| Build the Unreal editor target | `cmake --workflow --preset debug-game` |
| Build and launch the editor | `coj editor` |
| Generate IDE project files | `coj unreal project-files` |
| Run native simulation tests | `cmake --workflow --preset native-simulation-tests` |
| Build and run Unreal unit tests | `cmake --workflow --preset debug-game-unit-tests` |

[Debug and package the game](docs/build-and-test.md) · [Run benchmarks](docs/benchmarks.md) · [Capture a Tracy profile](docs/profiling.md)

## Repository map

| Area | Purpose | Guide |
| --- | --- | --- |
| `cmake/` | CMake wrapper, presets, and Unreal orchestration | [CMake guide](cmake/README.md) |
| `PowerShell/` | coj installation, packaging, and script checks | [PowerShell guide](PowerShell/README.md) |
| `native/` | Standalone C++ libraries, tools, simulations, and tests | [Native guide](native/README.md) |
| `lispb/` | Generation targets and sources for C++, Slate, kernels, and materials | [Lispb guide](native/lispb/README.md) |
| `Source/` | Project Unreal modules and their adapters/tests | [Source map](Source/README.md) |
| `Plugins/` | Reusable engine extensions and game plugins | [Plugin map](Plugins/README.md) |
| `LevelScripts/` | S7-authored campaign, showcase, and benchmark scenarios | [Level scripts guide](LevelScripts/README.md) |
| `tools/` | Developer tools | [Tools guide](tools/README.md) |
| `docs/` | Build, test, benchmarking, and profiling guides | [Documentation hub](docs/README.md) |
