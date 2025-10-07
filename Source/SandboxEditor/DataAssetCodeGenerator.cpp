#include "SandboxEditor/DataAssetCodeGenerator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "Sandbox/macros/null_checks.hpp"

// Static method - creates instance and calls generate()
bool FDataAssetCodeGenerator::generate_asset_registry(FString const& scan_path,
                                                      UClass* asset_class,
                                                      FString const& generated_class_name,
                                                      FString const& output_directory,
                                                      TArray<FString> const& additional_includes,
                                                      FString const& namespace_name) {
    RETURN_VALUE_IF_NULLPTR(asset_class, false);

    FDataAssetCodeGenerator generator{scan_path,
                                      asset_class,
                                      generated_class_name,
                                      output_directory,
                                      additional_includes,
                                      namespace_name};
    return generator.generate();
}

// Private constructor - stores inputs as member variables
FDataAssetCodeGenerator::FDataAssetCodeGenerator(FString const& scan_path,
                                                 UClass* asset_class,
                                                 FString const& generated_class_name,
                                                 FString const& output_directory,
                                                 TArray<FString> const& additional_includes,
                                                 FString const& namespace_name)
    : scan_path(scan_path)
    , asset_class(asset_class)
    , generated_class_name(generated_class_name)
    , output_directory(output_directory)
    , additional_includes(additional_includes)
    , namespace_name(namespace_name)
    , asset_type_name(FString(asset_class->GetPrefixCPP()) + asset_class->GetName())
    , relative_header_path(TEXT("Sandbox/generated/data_asset_registries/") + generated_class_name +
                           TEXT(".h")) {}

// Instance method - performs the generation
bool FDataAssetCodeGenerator::generate() {
    constexpr auto logger{NestedLogger<"generate">()};

    logger.log_log(TEXT("Scanning '%s' for assets of type '%s'"), *scan_path, *asset_type_name);

    // Scan for assets
    auto const assets{scan_assets()};
    if (assets.Num() == 0) {
        logger.log_warning(TEXT("No assets found in '%s'"), *scan_path);
        return false;
    }

    logger.log_log(TEXT("Found %d assets"), assets.Num());

    // Ensure output directory exists
    auto const full_output_path{FPaths::ConvertRelativePathToFull(output_directory)};
    if (!IFileManager::Get().DirectoryExists(*full_output_path)) {
        logger.log_log(TEXT("Creating directory '%s'"), *full_output_path);
        if (!IFileManager::Get().MakeDirectory(*full_output_path, true)) {
            logger.log_error(TEXT("Failed to create directory '%s'"), *full_output_path);
            return false;
        }
    }

    // Generate code content
    auto const header_content{generate_header_content(assets)};
    auto const cpp_content{generate_cpp_content(assets)};

    // Write files (only if different)
    auto const header_path{full_output_path / generated_class_name + TEXT(".h")};
    auto const cpp_path{full_output_path / generated_class_name + TEXT(".cpp")};

    if (!write_file_if_different(header_path, header_content)) {
        logger.log_error(TEXT("Failed to write header file"));
        return false;
    }

    if (!write_file_if_different(cpp_path, cpp_content)) {
        logger.log_error(TEXT("Failed to write cpp file"));
        return false;
    }

    // Calculate project-relative path for output message
    FString const project_dir{FPaths::ProjectDir()};
    FString relative_output_dir{output_directory};
    if (output_directory.StartsWith(project_dir)) {
        relative_output_dir = output_directory.RightChop(project_dir.Len());
    }

    logger.log_log(TEXT("Generated %s in %s"), *generated_class_name, *relative_output_dir);

    return true;
}

TArray<FDataAssetCodeGenerator::FAssetInfo> FDataAssetCodeGenerator::scan_assets() {
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

FString FDataAssetCodeGenerator::generate_header_content(TArray<FAssetInfo> const& assets) {
    FString content{};

    // Header guard and includes
    content += TEXT("#pragma once\n\n");
    content += TEXT("#include \"CoreMinimal.h\"\n");

    // Add additional includes
    for (auto const& include : additional_includes) {
        content += FString::Printf(TEXT("#include \"%s\"\n"), *include);
    }

    content += TEXT("\n");

    // Forward declare if no includes provided
    if (additional_includes.Num() == 0) {
        content += FString::Printf(TEXT("class %s;\n\n"), *asset_type_name);
    }

    // Open namespace if specified
    bool const has_namespace{!namespace_name.IsEmpty()};
    if (has_namespace) {
        content += FString::Printf(TEXT("namespace %s {\n\n"), *namespace_name);
    }

    // Class declaration
    content += TEXT("/**\n");
    content += TEXT(" * GENERATED CODE - DO NOT EDIT MANUALLY\n");
    content += TEXT(" * Auto-generated accessor functions for data assets.\n");
    content += TEXT(" * Regenerate using the SandboxEditor toolbar button.\n");
    content += TEXT(" */\n");
    content += FString::Printf(TEXT("class %s {\n"), *generated_class_name);
    content += TEXT("  public:\n");

    // Generate accessor function declarations
    for (auto const& asset : assets) {
        content +=
            FString::Printf(TEXT("    static %s* %s();\n"), *asset_type_name, *asset.function_name);
    }

    content += TEXT("};\n");

    // Close namespace if specified
    if (has_namespace) {
        content += FString::Printf(TEXT("\n}  // namespace %s\n"), *namespace_name);
    }

    return content;
}

FString FDataAssetCodeGenerator::generate_cpp_content(TArray<FAssetInfo> const& assets) {
    FString content{};

    // Includes
    content += FString::Printf(TEXT("#include \"%s\"\n\n"), *relative_header_path);
    content += TEXT("#include \"Engine/StreamableManager.h\"\n");
    content += TEXT("#include \"UObject/ConstructorHelpers.h\"\n\n");

    // Open namespace if specified
    bool const has_namespace{!namespace_name.IsEmpty()};
    if (has_namespace) {
        content += FString::Printf(TEXT("namespace %s {\n\n"), *namespace_name);
    }

    // Generate accessor function implementations
    for (auto const& asset : assets) {
        content += FString::Printf(TEXT("%s* %s::%s() {\n"),
                                   *asset_type_name,
                                   *generated_class_name,
                                   *asset.function_name);

        content +=
            FString::Printf(TEXT("    static %s* cached_asset{nullptr};\n"), *asset_type_name);

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

    // Close namespace if specified
    if (has_namespace) {
        content += FString::Printf(TEXT("}  // namespace %s\n"), *namespace_name);
    }

    return content;
}

bool FDataAssetCodeGenerator::write_file_if_different(FString const& file_path,
                                                      FString const& content) {
    constexpr auto logger{NestedLogger<"write_file_if_different">()};

    // Check if file exists and read its contents
    if (IFileManager::Get().FileExists(*file_path)) {
        FString existing_content{};
        if (FFileHelper::LoadFileToString(existing_content, *file_path)) {
            // Compare content
            if (existing_content.Equals(content)) {
                logger.log_verbose(TEXT("File unchanged, skipping write: '%s'"), *file_path);
                return true;
            }
            logger.log_log(TEXT("File content changed, updating: '%s'"), *file_path);
        } else {
            logger.log_warning(TEXT("Failed to read existing file: '%s'"), *file_path);
        }
    } else {
        logger.log_log(TEXT("Creating new file: '%s'"), *file_path);
    }

    // Write file with new content
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
