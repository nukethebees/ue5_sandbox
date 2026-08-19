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
