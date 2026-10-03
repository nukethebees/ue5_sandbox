# Sandbox Unreal Engine Project

Personal Unreal Engine 5.8 space-combat sandbox. This repository contains the game, standalone
native simulation libraries, code generation, custom plugins, and the tooling used to build,
test, and measure them.

## Start here

Set `UE_ROOT` to a usable Unreal Engine installation, then prepare the worktree:

```powershell
coj prepare-worktree
```

The maintainer runs `pwsh -NoProfile -File PowerShell/InstallCoj.ps1` and manages PATH;
agents begin new tasks with `coj prepare-worktree`.
This clears the worktree's build output, initializes/updates submodules, and regenerates presets
and code. It does not run a broad project/test build; build only the task's relevant targets afterward.
Install shared per-user build tools with `coj install-central-tools` when needed.
Use `cmake --workflow --preset debug-game` to build the Editor and
`debug-game-unit-tests` only for explicit Unreal-enabled integration validation.
Native presets set `IOJ_WITH_UNREAL=OFF`.

For complete setup, build, testing, debugging, and packaging instructions, see
[Build and test](docs/build-and-test.md).

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
