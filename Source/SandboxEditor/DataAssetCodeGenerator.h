#pragma once

#include "CoreMinimal.h"

class UClass;

/**
 * Editor utility for generating C++ accessor code for data assets.
 * Scans a directory for UPrimaryDataAsset instances and generates a static
 * accessor class with functions that return references to these assets.
 */
class FDataAssetCodeGenerator {
  public:
    /**
     * Generate C++ code files for data asset accessors.
     * @param scan_path Package path to scan (e.g., "/Game/DataAssets/Bullets/")
     * @param asset_class Class filter for assets (e.g., UBulletDataAsset::StaticClass())
     * @param generated_class_name Name for the generated class (e.g., "BulletAssetRegistry")
     * @param output_directory Directory for generated files (e.g., "Source/Sandbox/generated/")
     * @return true if generation succeeded, false otherwise
     */
    static bool generate_asset_registry(FString const& scan_path,
                                        UClass* asset_class,
                                        FString const& generated_class_name,
                                        FString const& output_directory);
  private:
    struct FAssetInfo {
        FString asset_name;
        FString asset_path;
        FString function_name;
    };

    static TArray<FAssetInfo> scan_assets(FString const& scan_path, UClass* asset_class);
    static FString generate_header_content(TArray<FAssetInfo> const& assets,
                                           FString const& class_name,
                                           FString const& asset_type_name);
    static FString generate_cpp_content(TArray<FAssetInfo> const& assets,
                                        FString const& class_name,
                                        FString const& asset_type_name);
    static bool write_file(FString const& file_path, FString const& content);
    static FString sanitize_function_name(FString const& asset_name);
};
