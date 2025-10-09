# Strong Typedefs Directory

This directory contains auto-generated strong typedef wrapper structs created by the typedef code generator.

## What are Strong Typedefs?

Strong typedefs are type-safe wrapper structs that provide compile-time type checking while maintaining full Unreal Engine reflection support. Unlike simple type aliases (`using Ammo = float;`), strong typedefs prevent accidental type mixing and enable UPROPERTY reflection.

## Structure

Each typedef generates a single header file containing:
- A USTRUCT wrapper with private `value` member
- Explicit constructor and conversion operators
- Operator overloads (comparison, arithmetic, etc.)
- Member function forwarding via variadic templates
- Hash support for TMap/TSet usage
- Blueprint read-only exposure

## How to Generate

1. Open the project in Unreal Editor
2. Look for the "Generate Typedefs" button in the Level Editor toolbar (Sandbox Tools section)
3. Click the button to regenerate all typedef wrappers

## Configuration

Typedefs are defined in `codegen/typedef_definitions.py`:
- Add new TypedefSpec entries to the TYPEDEFS array
- Configure operators, members, and options per typedef
- Run generation to create wrapper structs

## Important Notes

- **DO NOT EDIT** generated files manually - they will be overwritten on regeneration
- Generated files are **committed to version control** for build reproducibility
- The generator only writes files when content has changed (incremental updates)
- Files are automatically formatted with clang-format after generation

## Example Usage

```cpp
// Old weak typedef (no type safety):
using Ammo = float;
Ammo bullets = 5.0f;
float health = bullets;  // Oops! Accidentally mixed types

// New strong typedef (type safe):
FAmmoStrong bullets{5.0f};
FHealthStrong health = bullets;  // Compile error! Types are incompatible
float bullets_value = bullets.get_value();  // Explicit conversion required
```

## Generated Files

Current strong typedefs:
- **Ammo.h** - Wrapper for `float` (ammo counts)
- **StackSize.h** - Wrapper for `uint32` (inventory stack sizes)

## Customization

To modify generation behavior, see:
- `codegen/strong_typedefs.py` - Generator implementation
- `codegen/typedef_definitions.py` - Typedef specifications
- `Source/SandboxEditor/TypedefCodeGenerator.cpp` - Editor integration
