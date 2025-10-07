#include "SandboxEditor/DataAssetCodeGenerator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

bool FDataAssetCodeGenerator::generate_asset_registry(FString const& scan_path,
                                                      UClass* asset_class,
                                                      FString const& generated_class_name,
                                                      FString const& output_directory) {
    constexpr auto logger{NestedLogger<"generate_asset_registry">()};

    if (!asset_class) {
        logger.log_error(TEXT("Invalid asset class"));
        return false;
    }

    logger.log_log(
        TEXT("Scanning '%s' for assets of type '%s'"), *scan_path, *asset_class->GetName());

    // Scan for assets
    auto const assets{scan_assets(scan_path, asset_class)};
    if (assets.Num() == 0) {
        logger.log_warning(TEXT("No assets found in '%s'"), *scan_path);
        return false;
    }

    logger.log_log(TEXT("Found %d assets"), assets.Num());

    // Generate code content
    auto const asset_type_name{asset_class->GetName()};
    auto const header_content{
        generate_header_content(assets, generated_class_name, asset_type_name)};
    auto const cpp_content{generate_cpp_content(assets, generated_class_name, asset_type_name)};

    // Ensure output directory exists
    auto const full_output_path{FPaths::ConvertRelativePathToFull(output_directory)};
    if (!IFileManager::Get().DirectoryExists(*full_output_path)) {
        logger.log_log(TEXT("Creating directory '%s'"), *full_output_path);
        if (!IFileManager::Get().MakeDirectory(*full_output_path, true)) {
            logger.log_error(TEXT("Failed to create directory '%s'"), *full_output_path);
            return false;
        }
    }

    // Write files
    auto const header_path{full_output_path / generated_class_name + TEXT(".h")};
    auto const cpp_path{full_output_path / generated_class_name + TEXT(".cpp")};

    if (!write_file(header_path, header_content)) {
        logger.log_error(TEXT("Failed to write header file"));
        return false;
    }

    if (!write_file(cpp_path, cpp_content)) {
        logger.log_error(TEXT("Failed to write cpp file"));
        return false;
    }

    logger.log_log(TEXT("Successfully generated files:\n  %s\n  %s"), *header_path, *cpp_path);

    return true;
}

TArray<FDataAssetCodeGenerator::FAssetInfo>
    FDataAssetCodeGenerator::scan_assets(FString const& scan_path, UClass* asset_class) {
    constexpr auto logger{NestedLogger<"scan_assets">()};
    TArray<FAssetInfo> result{};

    // Get the asset registry module
    auto& asset_registry_module{
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry")};
    auto& asset_registry{asset_registry_module.Get()};

    // Create filter for the specified path and class
    FARFilter filter{};
    filter.PackagePaths.Add(FName(*scan_path));
    filter.ClassPaths.Add(asset_class->GetClassPathName());
    filter.bRecursivePaths = true;
    filter.bRecursiveClasses = false;

    // Get matching assets
    TArray<FAssetData> asset_data{};
    asset_registry.GetAssets(filter, asset_data);

    // Convert to our internal format
    for (auto const& data : asset_data) {
        FAssetInfo info{};
        info.asset_name = data.AssetName.ToString();
        info.asset_path = data.GetObjectPathString();
        info.function_name = sanitize_function_name(info.asset_name);
        result.Add(info);

        logger.log_verbose(TEXT("Found: %s (%s) -> %s()"),
                           *info.asset_name,
                           *info.asset_path,
                           *info.function_name);
    }

    return result;
}

FString FDataAssetCodeGenerator::generate_header_content(TArray<FAssetInfo> const& assets,
                                                         FString const& class_name,
                                                         FString const& asset_type_name) {
    FString content{};

    // Header guard and includes
    content += TEXT("#pragma once\n\n");
    content += TEXT("#include \"CoreMinimal.h\"\n\n");
    content += FString::Printf(TEXT("class %s;\n\n"), *asset_type_name);

    // Class declaration
    content += TEXT("/**\n");
    content += TEXT(" * GENERATED CODE - DO NOT EDIT MANUALLY\n");
    content += TEXT(" * Auto-generated accessor functions for data assets.\n");
    content += TEXT(" * Regenerate using the SandboxEditor toolbar button.\n");
    content += TEXT(" */\n");
    content += FString::Printf(TEXT("class %s {\n"), *class_name);
    content += TEXT("  public:\n");

    // Generate accessor function declarations
    for (auto const& asset : assets) {
        content += FString::Printf(
            TEXT("    static %s const* %s();\n"), *asset_type_name, *asset.function_name);
    }

    content += TEXT("};\n");

    return content;
}

FString FDataAssetCodeGenerator::generate_cpp_content(TArray<FAssetInfo> const& assets,
                                                      FString const& class_name,
                                                      FString const& asset_type_name) {
    FString content{};

    // Includes
    content += FString::Printf(TEXT("#include \"Sandbox/generated/%s.h\"\n\n"), *class_name);
    content += TEXT("#include \"Engine/StreamableManager.h\"\n");
    content += TEXT("#include \"UObject/ConstructorHelpers.h\"\n\n");

    // Generate accessor function implementations
    for (auto const& asset : assets) {
        content += FString::Printf(
            TEXT("%s const* %s::%s() {\n"), *asset_type_name, *class_name, *asset.function_name);

        content += FString::Printf(TEXT("    static %s const* cached_asset{nullptr};\n"),
                                   *asset_type_name);

        content += TEXT("    if (!cached_asset) {\n");
        content +=
            FString::Printf(TEXT("        cached_asset = LoadObject<%s>(nullptr, TEXT(\"%s\"));\n"),
                            *asset_type_name,
                            *asset.asset_path);
        content += TEXT("        if (!cached_asset) {\n");
        content += FString::Printf(
            TEXT("            UE_LOG(LogTemp, Error, TEXT(\"Failed to load asset: %s\"));\n"),
            *asset.asset_path);
        content += TEXT("        }\n");
        content += TEXT("    }\n");
        content += TEXT("    return cached_asset;\n");
        content += TEXT("}\n\n");
    }

    return content;
}

bool FDataAssetCodeGenerator::write_file(FString const& file_path, FString const& content) {
    constexpr auto logger{NestedLogger<"write_file">()};

    if (FFileHelper::SaveStringToFile(
            content, *file_path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) {
        logger.log_log(TEXT("Wrote file '%s'"), *file_path);
        return true;
    }

    logger.log_error(TEXT("Failed to write file '%s'"), *file_path);
    return false;
}

FString FDataAssetCodeGenerator::sanitize_function_name(FString const& asset_name) {
    FString sanitized{asset_name};

    // Replace spaces and special characters with underscores
    sanitized = sanitized.Replace(TEXT(" "), TEXT("_"));
    sanitized = sanitized.Replace(TEXT("-"), TEXT("_"));
    sanitized = sanitized.Replace(TEXT("."), TEXT("_"));

    // Convert to lowercase for snake_case
    sanitized = sanitized.ToLower();

    // Prepend "get_" for clarity
    return TEXT("get_") + sanitized;
}
