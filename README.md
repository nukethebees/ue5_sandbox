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

Generated C++ files are defined in `Codegen/manifest.py`. After changing the manifest or
Codegen implementation, regenerate them from the repository root:

```bash
python3 -m Codegen.generate
```

To verify that committed generated files are current without writing them:

```bash
python3 -m Codegen.generate --check
```

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

With `UE_ROOT` available, configure and build the Development Editor target:

```bash
cmake --preset development
cmake --build --preset development
```

The `debug`, `debug-game`, `development`, `shipping`, and `test` presets select their
matching Unreal configuration and build `editor`. Each also has a workflow preset, for
example `cmake --workflow --preset development`, that configures and builds it in one
command. The `game` target remains available through a build preset, for example
`cmake --build --preset development --target game`.
