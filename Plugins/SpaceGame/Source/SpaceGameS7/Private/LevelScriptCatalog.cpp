#include <SpaceGameS7/LevelScriptCatalog.h>

#include <sandbox/level_authoring/CatalogValidation.h>
#include <SandboxCoreEngine/strings.h>
#include <SpaceGame/levels/NativeLevelDefinitionConversion.h>
#include <SpaceGameS7/CampaignDefinitionReader.h>
#include <SpaceGameS7/LevelDefinitionReader.h>

#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

namespace ml::s7 {
namespace {
void append_error(FString& errors, FString message) {
    if (!errors.IsEmpty()) {
        errors += TEXT("\n");
    }
    errors += MoveTemp(message);
}

auto to_utf8(FString const& value) -> std::string {
    return TCHAR_TO_UTF8(*value);
}

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

auto format_read_error(FCampaignDefinitionReadResult const& result) -> FString {
    if (!result.script_error.IsEmpty()) {
        return result.script_error;
    }

    TArray<FString> messages;
    messages.Reserve(result.decode_errors.Num());
    for (auto const& error : result.decode_errors) {
        messages.Add(FString::Printf(TEXT("%s: %s"), *error.path, *error.message));
    }
    return FString::Join(messages, TEXT("\n"));
}

auto to_native_levels(TArray<FLevelScriptEntry> const& entries)
    -> std::vector<level_authoring::LevelCatalogEntry> {
    std::vector<level_authoring::LevelCatalogEntry> result;
    result.reserve(entries.Num());
    for (auto const& entry : entries) {
        level_authoring::LevelCatalogEntry native{.filename = to_utf8(entry.filename)};
        if (entry.definition) {
            native.definition = level_authoring::to_native(*entry.definition);
        }
        result.push_back(std::move(native));
    }
    return result;
}

auto to_native_campaign(FCampaignDefinition const& definition)
    -> level_authoring::CampaignDefinition {
    level_authoring::CampaignDefinition result{
        .id = to_utf8(definition.id.value.ToString().ToLower()),
        .title = to_utf8(definition.title),
    };
    result.level_ids.reserve(definition.level_ids.Num());
    for (auto const level_id : definition.level_ids) {
        result.level_ids.push_back(to_utf8(level_id.value.ToString().ToLower()));
    }
    return result;
}

auto to_native_campaigns(TArray<FCampaignScriptEntry> const& entries)
    -> std::vector<level_authoring::CampaignCatalogEntry> {
    std::vector<level_authoring::CampaignCatalogEntry> result;
    result.reserve(entries.Num());
    for (auto const& entry : entries) {
        level_authoring::CampaignCatalogEntry native{.filename = to_utf8(entry.filename)};
        if (entry.definition) {
            native.definition = to_native_campaign(*entry.definition);
        }
        result.push_back(std::move(native));
    }
    return result;
}

void apply_level_issues(FLevelScriptCatalogResult& result) {
    auto const native_entries{to_native_levels(result.entries)};
    auto const issues{level_authoring::validate_level_catalog(native_entries)};
    for (auto const& issue : issues) {
        auto& entry{result.entries[static_cast<int32>(issue.entry_index)]};
        entry.error = ml::to_fstring(issue.message);
        entry.definition.Reset();
        append_error(result.error, FString::Printf(TEXT("%s: %s"), *entry.filename, *entry.error));
    }
}

void apply_campaign_issues(FLevelScriptCatalogResult& result) {
    auto const levels{to_native_levels(result.entries)};
    auto const campaigns{to_native_campaigns(result.campaigns)};
    auto const issues{level_authoring::validate_campaign_catalog(campaigns, levels, {})};
    for (auto const& issue : issues) {
        auto& entry{result.campaigns[static_cast<int32>(issue.entry_index)]};
        entry.error = ml::to_fstring(issue.message);
        entry.definition.Reset();
        append_error(result.error, FString::Printf(TEXT("%s: %s"), *entry.filename, *entry.error));
    }
}

void discover_campaigns(FLevelScriptCatalogResult& result) {
    auto const campaign_directory{FPaths::Combine(result.directory, TEXT("Campaigns"))};
    auto& file_manager{IFileManager::Get()};
    if (!file_manager.DirectoryExists(*campaign_directory)) {
        return;
    }

    TArray<FString> filenames;
    file_manager.FindFiles(
        filenames, *FPaths::Combine(campaign_directory, TEXT("*.scm")), true, false);
    filenames.Sort([](FString const& lhs, FString const& rhs) {
        return lhs.Compare(rhs, ESearchCase::IgnoreCase) < 0;
    });

    FCampaignDefinitionReader reader{FPaths::Combine(result.directory, TEXT("Libraries"))};
    result.campaigns.Reserve(filenames.Num());
    for (auto const& filename : filenames) {
        auto const path{FPaths::Combine(campaign_directory, filename)};
        FCampaignScriptEntry entry{.filename = filename, .path = path};
        auto read_result{reader.read_file(path)};
        if (!read_result) {
            entry.error = format_read_error(read_result);
            append_error(result.error, FString::Printf(TEXT("%s: %s"), *filename, *entry.error));
        } else {
            entry.definition = MoveTemp(read_result.definition);
        }
        result.campaigns.Add(MoveTemp(entry));
    }

    apply_campaign_issues(result);
    result.campaigns.Sort([](FCampaignScriptEntry const& lhs, FCampaignScriptEntry const& rhs) {
        if (lhs && rhs) {
            auto const title_order{
                lhs.definition->title.Compare(rhs.definition->title, ESearchCase::IgnoreCase)};
            if (title_order != 0) {
                return title_order < 0;
            }
            return lhs.definition->id.value.LexicalLess(rhs.definition->id.value);
        }
        return static_cast<bool>(lhs) && !static_cast<bool>(rhs);
    });
}
} // namespace

auto catalog_category(FLevelDefinition const& definition) noexcept -> ELevelCatalogCategory {
    return definition.player_entity_id.is_set() ? ELevelCatalogCategory::Mission
                                                : ELevelCatalogCategory::BattleViewer;
}

auto default_level_script_directory() -> FString {
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectDir(), TEXT("LevelScripts")));
}

auto default_campaign_script_directory() -> FString {
    return FPaths::Combine(default_level_script_directory(), TEXT("Campaigns"));
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

    FLevelDefinitionReader reader{FPaths::Combine(result.directory, TEXT("Libraries"))};
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

    apply_level_issues(result);
    discover_campaigns(result);
    return result;
}

auto discover_level_scripts() -> FLevelScriptCatalogResult {
    return discover_level_scripts(default_level_script_directory());
}
} // namespace ml::s7
