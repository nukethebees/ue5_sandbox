#include <SpaceGameS7/LevelScriptCatalog.h>

#include <SpaceGameS7/LevelDefinitionReader.h>

#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

namespace ml::s7 {
namespace {
auto format_read_error(FLevelDefinitionReadResult const& result) -> FString {
    if (!result.script_error.IsEmpty()) {
        return result.script_error;
    }

    TArray<FString> messages;
    messages.Reserve(result.decode_errors.Num() + result.validation_errors.Num());
    for (auto const& error : result.decode_errors) {
        messages.Add(FString::Printf(TEXT("%s: %s"), *error.path, *error.message));
    }
    for (auto const& error : result.validation_errors) {
        messages.Add(error.message);
    }
    return FString::Join(messages, TEXT("\n"));
}
}

auto default_level_script_directory() -> FString {
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts")));
}

auto discover_level_scripts(FStringView const directory) -> FLevelScriptCatalogResult {
    FLevelScriptCatalogResult result;
    result.directory = FPaths::ConvertRelativePathToFull(FString{directory});
    auto& file_manager{IFileManager::Get()};
    if (!file_manager.DirectoryExists(*result.directory)) {
        result.error =
            FString::Printf(TEXT("Level script directory does not exist: %s"), *result.directory);
        return result;
    }

    TArray<FString> filenames;
    file_manager.FindFiles(
        filenames, *FPaths::Combine(result.directory, TEXT("*.scm")), true, false);
    filenames.Sort([](FString const& lhs, FString const& rhs) {
        return lhs.Compare(rhs, ESearchCase::IgnoreCase) < 0;
    });

    FLevelDefinitionReader reader;
    result.entries.Reserve(filenames.Num());
    for (auto const& filename : filenames) {
        auto const path{FPaths::Combine(result.directory, filename)};
        FLevelScriptEntry entry{
            .filename = filename,
            .path = path,
            .display_title = FPaths::GetBaseFilename(filename),
        };
        if (!FFileHelper::LoadFileToString(entry.source_text, *path)) {
            entry.error = FString::Printf(TEXT("Could not read level script '%s'."), *path);
            result.entries.Add(MoveTemp(entry));
            continue;
        }

        auto read_result{reader.read_source(entry.source_text)};
        if (read_result) {
            entry.display_title = read_result.definition->metadata.title;
            entry.description = read_result.definition->metadata.description;
            entry.definition = MoveTemp(read_result.definition);
        } else {
            entry.error = format_read_error(read_result);
        }
        result.entries.Add(MoveTemp(entry));
    }

    return result;
}

auto discover_level_scripts() -> FLevelScriptCatalogResult {
    return discover_level_scripts(default_level_script_directory());
}
}
