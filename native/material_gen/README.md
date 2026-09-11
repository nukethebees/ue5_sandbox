# MaterialGen

MaterialGen parses a small S-expression material language into a stable, UObject-free graph IR.
The native `materialc` executable validates sources and prints their lowered IR without launching
Unreal. Unreal is only required to verify asset types, emit material expressions, compile shaders,
and save packages.

## Layout

- `lib/include/material_gen` contains the public native API and IR.
- `lib/src` contains parsing, semantic analysis, IR validation, and source hashing.
- `materialc` contains the native command-line frontend.
- `tests` contains GoogleTest coverage for the native library.

## Commands

Build the native compiler and validate the canonical material:

```powershell
cmake --build --preset codegen --target materialc
out/build/codegen/native/material_gen/materialc/materialc.exe validate --input Plugins/SandboxUI/Source/SandboxUI/Private/materials/UiGlowComposite.material.scm
```

Inspect the ordered parameters, stable node handles, bindings, outputs, and texture dependencies:

```powershell
out/build/codegen/native/material_gen/materialc/materialc.exe dump-ir --input Plugins/SandboxUI/Source/SandboxUI/Private/materials/UiGlowComposite.material.scm
```

The compiler locates the nearest parent directory containing a `.uproject`. Use
`--project-root <directory>` when the material source is outside the project tree. Native texture
validation checks that `/Game/...` or `/<Plugin>/...` resolves to an existing `.uasset`. Unreal
generation additionally verifies that the object is a texture.

The repository target performs native validation directly:

```powershell
cmake --build --preset debug-game --target validate-ui-glow-material
```

Generate and compile the Unreal material separately:

```powershell
cmake --build --preset debug-game --target generate-ui-glow-material
```

## Language

A source contains one `material` definition with an owned generated asset path, UI domain, additive
blend mode, parameters, sequential `let` bindings, and one emissive output. Expressions support
numeric literals, symbols, `float2`, `float3`, `float4`, `+`, `-`, `*`, `/`, `lerp`, `saturate`,
`texcoord`, `sample`, and `custom` HLSL.

See
`Plugins/SandboxUI/Source/SandboxUI/Private/materials/UiGlowComposite.material.scm` for the
canonical example.
