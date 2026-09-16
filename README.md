# Sandbox Unreal Engine Project

Personal Unreal Engine 5.8 space-combat sandbox. This repository contains the game, standalone
native simulation libraries, code generation, custom plugins, and the tooling used to build,
test, and measure them.

## Start here

Set `UE_ROOT` to a usable Unreal Engine installation, then load the project commands in a
PowerShell session:

```powershell
. .\dev.ps1
csetup
```

The leading dot keeps the commands available in the current session. Run `dev-help` for the
available navigation and build commands. `cplay debug-game` prepares and builds an Editor-ready
configuration; `cmake --workflow --preset debug-game-unit-tests` runs the fast unit suites.

For complete setup, build, testing, debugging, and packaging instructions, see
[Build and test](docs/build-and-test.md).

## Repository map

| Area | Purpose | Guide |
| --- | --- | --- |
| `cmake/` | CMake wrapper, presets, jobserver and Unreal orchestration | [CMake guide](cmake/README.md) |
| `PowerShell/` and `dev.ps1` | Returning-developer navigation and build commands | [PowerShell guide](PowerShell/README.md) |
| `native/` | Standalone C++ libraries, tools, simulations, and tests | [Native guide](native/README.md) |
| `Codegen/` and `lispb/` | Generated C++/Slate/material outputs and their inputs | [Code generation guide](Codegen/README.md) |
| `Source/` | Project Unreal modules and their adapters/tests | [Source map](Source/README.md) |
| `Plugins/` | Reusable engine extensions and game plugins | [Plugin map](Plugins/README.md) |
| `LevelScripts/` | S7-authored campaign, showcase, and benchmark scenarios | [Level scripts guide](LevelScripts/README.md) |
| `Scripts/` | Developer automation, benchmark runners, and analysis tools | [Scripts guide](Scripts/README.md) |
| `tools/` | Shared developer tools, including the jobserver | [Tools guide](tools/README.md) |
| `docs/` | Cross-cutting development and investigation documentation | [Documentation hub](docs/README.md) |

## Common destinations

- [Build, test, debug, and package](docs/build-and-test.md)
- [Run benchmarks correctly](docs/benchmarks.md)
- [Regenerate committed code](Codegen/README.md)
- [Author or find a level scenario](LevelScripts/README.md)
