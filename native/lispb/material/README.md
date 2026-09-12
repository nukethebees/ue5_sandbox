# MaterialGen

MaterialGen parses a small S-expression material language into a stable, UObject-free graph IR.
The native `lispb` executable validates sources, prints their lowered IR, and compiles a
deterministic material artifact without launching Unreal. Unreal is only required to verify asset
types, emit material expressions, compile shaders, and save packages.

## Layout

- `lib/include/material_gen` contains the public native API and IR.
- `lib/src` contains parsing, semantic analysis, IR validation, and source hashing.
- `compiler.cpp` contains the Material adapter linked into the unified `lispb` frontend.
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

The unified project file establishes the project root used for source and texture resolution.
Native texture validation checks that `/Game/...` or `/<Plugin>/...` resolves to an existing
`.uasset`. Unreal generation additionally verifies that the object is a texture.

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

A source contains one `material` definition. New assets are restricted to generated material
directories. `(adopt-existing true)` explicitly permits replacing an existing material at another
path and then records MaterialGen ownership metadata. UI, surface, and post-process domains are
supported, together with additive, translucent, opaque, and masked blends where valid. Surface
materials may select shading, two-sided rendering, disabled depth testing,
instanced-static-mesh usage, and Base Color, Emissive, Opacity, Opacity Mask, or World Position
Offset outputs.

Expressions support numeric literals, symbols, `float2`, `float3`, `float4`, `+`, `-`, `*`, `/`,
`lerp`, `saturate`, `time`, `sin`, `cos`, `texcoord`, `per-instance-custom-data`, texture parameters
and constants, `sample`, `swizzle`, common world-space values, world/local position transforms,
scene textures, and `custom` HLSL. `shader-call` is the typed hybrid boundary: it records a virtual
`.ush`/`.usf` include, function name, typed result, and typed semantic inputs while Unreal emits a
single Custom expression for the procedural function body. The surrounding arithmetic, masks,
sampling, and output routing remain visible as native expressions.

`+` and `*` accept two or more operands and remain n-ary operations in the material IR; the Unreal
backend lowers them to binary expression chains. `float2`, `float3`, and `float4` accept scalar
expressions as components while literal-only forms remain compact constant nodes. `(time)` returns
pause-respecting game time in seconds, and `(sin value)` / `(cos value)` use radians.

See
`Plugins/SandboxUI/Source/SandboxUI/Private/materials/UiGlowComposite.lispb` for the
canonical example.
The world-space per-instance example is
`Plugins/SpaceGame/Source/SpaceGamePresentation/Private/materials/SoftTargetWorld.lispb`.
Existing non-tutorial materials can be regenerated together with:

```powershell
cmake --build --preset debug-game --target generate-migrated-materials
```

## Unreal Engine 5.8 backend

MaterialGen creates built-in `UMaterialExpression` objects through `UMaterialEditingLibrary`, which
sets their material owner, adds them to the expression collection, assigns editor positions and
GUIDs, and registers parameter expressions. Regeneration validates the native IR and dependencies,
then completely replaces the expressions of a MaterialGen-owned asset so removed source nodes do
not remain serialized. `RecompileMaterial` finalizes the change and updates dependent material
instances before the package is saved.

UE 5.8 has two compiler paths for built-in expressions. The normal path calls
`Compile(FMaterialCompiler*)`; its arithmetic implementation folds constants and uniform
expressions and applies identities such as multiplication by zero or one. The newer path calls
`Build(MIR::FEmitter&)`, where the material IR emitter performs further operator folding and
simplification. `GenerateHLSLExpression(...)` and `GenerateHLSLStatements(...)`, which appear in
some older examples, are not present in the UE 5.8 source tree.

The new translator is guarded by `r.Material.Translator.EnableNew`, defaults to disabled, and the
editor describes it as experimental and incomplete. Generated materials therefore do not enable
`bEnableNewHLSLGenerator`; emitting built-in expression graphs supports both paths without taking a
dependency on the experimental MIR emitter API. Custom expressions also implement both compiler
interfaces, but their HLSL bodies remain opaque functions, so native nodes should be preferred for
semantics represented by the material IR.
