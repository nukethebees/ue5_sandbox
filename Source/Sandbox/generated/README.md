# Generated Code Directory

This directory contains auto-generated C++ code created by the SandboxEditor module.

## Contents

- **BulletAssetRegistry.h/cpp** - Auto-generated accessor functions for bullet data assets

## How to Generate

1. Open the project in Unreal Editor
2. Look for the "Generate Asset Code" button in the Level Editor toolbar (User section)
3. Click the button to regenerate the code

## Important Notes

- **DO NOT EDIT** files in this directory manually - they will be overwritten
- Generated files are excluded from version control via .gitignore
- If you need to modify the generation logic, see `Source/SandboxEditor/DataAssetCodeGenerator.cpp`

## Configuration

The generator is configured in `SandboxEditor.cpp`:
- **Scan path**: `/Game/DataAssets/Bullets/`
- **Asset class**: `UBulletDataAsset`
- **Generated class name**: `BulletAssetRegistry`

To add more generators or change settings, modify the `on_generate_data_asset_code()` function.
