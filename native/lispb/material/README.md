# MaterialGen

MaterialGen parses a small S-expression material language into a stable, UObject-free graph IR.
The native `lispb` executable validates sources, prints their lowered IR, and compiles a
deterministic material artifact without launching Unreal. Unreal is only required to verify asset
types, emit material expressions, compile shaders, and save packages.

## Layout

- `lib/include/material_gen` contains the public native API and IR.
- `lib/src` contains parsing, semantic analysis, IR validation, and source hashing.
- `cli` contains the Material adapter linked into the unified `lispb` frontend.
- `tests` contains GoogleTest coverage for the native library.

## Commands

Build the native compiler and validate the canonical material:

```powershell
cmake --build --preset codegen --target lispb
out/build/codegen/native/lispb/lispb.exe validate --target ui-glow-material
```

Inspect the ordered parameters, stable node handles, bindings, outputs, and texture dependencies:

```powershell
out/build/codegen/native/lispb/lispb.exe dump-ir --target ui-glow-material
```

Compile a versioned binary IR artifact:

```powershell
out/build/codegen/native/lispb/lispb.exe generate --target ui-glow-material
```

The artifact contains the project-relative source filename, SHA-256 source hash, validated IR, and
resolved texture dependencies. Repeated compilation of identical inputs produces identical bytes.
Pass `--depfile <file>` to emit Ninja-compatible source and texture dependency information.

The compiler locates the nearest parent directory containing a `.uproject`. Use
`--project-root <directory>` when the material source is outside the project tree. Native texture
validation checks that `/Game/...` or `/<Plugin>/...` resolves to an existing `.uasset`. Unreal
generation additionally verifies that the object is a texture.

The repository target performs native validation directly:

```powershell
cmake --build --preset debug-game --target validate-ui-glow-material
```

Compile only the canonical artifact:

```powershell
cmake --build --preset debug-game --target compile-ui-glow-material
```

Validate the compiled artifact and texture object types through Unreal without mutating the
material:

```powershell
cmake --build --preset debug-game --target validate-ui-glow-material-unreal
```

Generate and compile the Unreal material separately:

```powershell
cmake --build --preset debug-game --target generate-ui-glow-material
```

## Language

A source contains one `material` definition with an owned generated asset path. UI/additive and
surface/additive or surface/translucent materials are supported. Surface materials may select
`unlit` shading, two-sided rendering, disabled depth testing, instanced-static-mesh usage, and an
opacity output. Expressions support numeric literals, symbols, `float2`, `float3`, `float4`, `+`,
`-`, `*`, `/`, `lerp`, `saturate`, `texcoord`, `per-instance-custom-data`, `sample`, and `custom`
HLSL.

See
`Plugins/SandboxUI/Source/SandboxUI/Private/materials/UiGlowComposite.lispb` for the
canonical example.
The world-space per-instance example is
`Plugins/SpaceGame/Source/SpaceGamePresentation/Private/materials/SoftTargetWorld.lispb`.
