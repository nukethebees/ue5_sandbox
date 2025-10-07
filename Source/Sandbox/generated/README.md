# Generated Code Directory

This directory contains auto-generated C++ code created by the SandboxEditor module.

## Structure

- **data_asset_registries/** - Auto-generated data asset accessor classes
  - **BulletAssetRegistry.h/cpp** - Accessor functions for bullet data assets

## How to Generate

1. Open the project in Unreal Editor
2. Look for the "Generate Asset Code" button in the Level Editor toolbar (User section)
3. Click the button to regenerate the code

## Important Notes

- **DO NOT EDIT** generated files manually - they will be overwritten on regeneration
- Generated files are **committed to version control** for build reproducibility
- The generator only writes files when content has changed (incremental updates)
- If you need to modify the generation logic, see `Source/SandboxEditor/DataAssetCodeGenerator.cpp`

## Configuration

The generator is configured in `SandboxEditor.cpp`:
- **Scan path**: `/Game/DataAssets/`
- **Asset class**: `UBulletDataAsset`
- **Generated class name**: `BulletAssetRegistry`
- **Output directory**: `Source/Sandbox/generated/data_asset_registries/`
- **Namespace**: `sandbox`

To add more generators or change settings, modify the `on_generate_data_asset_code()` function.
