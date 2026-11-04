# Sandbox Unreal Engine Project

Personal Unreal Engine 5.8 space-combat sandbox. This repository contains the game, standalone
native simulation libraries, code generation, custom plugins, and the tooling used to build,
test, and measure them.

## Start here

### Prerequisites

- Windows with Developer Mode enabled
- Rust via rustup (the repository pins the toolchain)
- Clang 24, including `clang-cl` and `clang-format`
- Visual Studio C++ tools: MSVC 14.50.35717 and Windows SDK 10.0.22621.0
- CMake 4.4.2+, Ninja, ccache, Python 3, Git, and PowerShell 7
- Unreal Engine 5.8

Set `UE_ROOT` to the engine directory and `IOJ_ROOT` to a shared tools directory
(for example, `C:\dev\ioj`). Add `%IOJ_ROOT%\tools\bin` and the command-line tools to PATH.

### Building tools

Build and install the core tools from the repository root:

```powershell
pwsh -NoProfile -File PowerShell/InstallCoj.ps1
coj prepare-worktree
coj install central-tools
```

`coj` is a general purpose tool for repo management.

### Building the game

#### Build only

```powershell
cmake --workflow --preset native-lean
cmake --workflow --preset debug-game
```

#### Build and test

```powershell
# Native simulation and tests
cmake --workflow --preset native-simulation-tests
# Unreal Editor and unit tests
cmake --workflow --preset debug-game-unit-tests
```

## Repository map

| Area | Purpose | Guide |
| --- | --- | --- |
| `cmake/` | CMake wrapper, presets, and Unreal orchestration | [CMake guide](cmake/README.md) |
| `PowerShell/` | coj installation, packaging, and script checks | [PowerShell guide](PowerShell/README.md) |
| `native/` | Standalone C++ libraries, tools, simulations, and tests | [Native guide](native/README.md) |
| `Codegen/` and `lispb/` | Generated C++/Slate/material outputs and their inputs | [Code generation guide](Codegen/README.md) |
| `Source/` | Project Unreal modules and their adapters/tests | [Source map](Source/README.md) |
| `Plugins/` | Reusable engine extensions and game plugins | [Plugin map](Plugins/README.md) |
| `LevelScripts/` | S7-authored campaign, showcase, and benchmark scenarios | [Level scripts guide](LevelScripts/README.md) |
| `tools/` | Shared developer tools, including the jobserver | [Tools guide](tools/README.md) |
| `docs/` | Cross-cutting development and investigation documentation | [Documentation hub](docs/README.md) |

## Common destinations

- [Build, test, debug, and package](docs/build-and-test.md)
- [Run benchmarks correctly](docs/benchmarks.md)
- [Profile native level benchmarks with Tracy](docs/profiling.md)
- [Regenerate committed code](Codegen/README.md)
- [Author or find a level scenario](LevelScripts/README.md)
