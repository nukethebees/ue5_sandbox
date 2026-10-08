#include <SpaceGameS7/level_script_catalog.h>

#include <ioj/levels/catalog_validation.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>
#include <SpaceGameS7/definition_reader.h>

#include <SandboxCoreEngine/strings.h>

#include <Containers/StringConv.h>
#include <HAL/FileManager.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

namespace ioj::levels::authoring {
namespace {
void append_error(FString& errors, FString message) {
    if (!errors.IsEmpty()) {
        errors += TEXT("\n");
    }
    errors += MoveTemp(message);
}

auto to_utf8(FString const& value) -> std::string {
    auto const utf8{FTCHARToUTF8{*value, value.Len()}};
    return {utf8.Get(), static_cast<std::size_t>(utf8.Length())};
}

auto to_native_levels(TArray<FLevelScriptEntry> const& entries) -> std::vector<LevelCatalogEntry> {
    std::vector<LevelCatalogEntry> result;
    auto const count{entries.Num()};
    for (int32 index{}; index < count; ++index) {
        auto const& entry{entries[index]};
        if (entry.definition) {
            auto definition{to_native(*entry.definition)};
            // Catalog entries have already passed decoding and semantic validation.
            check(definition.has_value());
            result.emplace_back(static_cast<CatalogEntryIndex>(index),
                                std::filesystem::path{*entry.path},
                                std::move(*definition));
        }
    }
    return result;
}
auto to_native_campaign(ml::FCampaignDefinition const& definition) -> CampaignDefinition {
    CampaignDefinition result{.id = CampaignId{to_utf8(definition.id.value.ToString().ToLower())},
                              .title = to_utf8(definition.title)};
    for (auto const id : definition.level_ids) {
        result.level_ids.emplace_back(to_utf8(id.value.ToString().ToLower()));
    }
    return result;
}
auto to_native_campaigns(TArray<FCampaignScriptEntry> const& entries)
    -> std::vector<CampaignCatalogEntry> {
    std::vector<CampaignCatalogEntry> result;
    auto const count{entries.Num()};
    for (int32 index{}; index < count; ++index) {
        auto const& entry{entries[index]};
        if (entry.definition) {
            result.push_back({static_cast<CatalogEntryIndex>(index),
                              std::filesystem::path{*entry.path},
                              to_native_campaign(*entry.definition)});
        }
    }
    return result;
}

void apply_level_issues(FLevelScriptCatalogResult& result) {
    auto const native_entries{to_native_levels(result.entries)};
    auto const issues{::ioj::levels::validate_level_catalog(native_entries)};
    for (auto const& issue : issues) {
        auto& entry{result.entries[static_cast<int32>(issue.entry_index)]};
        auto const message{ml::to_fstring(issue.diagnostic.message)};
        append_error(entry.error, message);
        entry.definition.Reset();
        append_error(result.error, FString::Printf(TEXT("%s: %s"), *entry.filename, *message));
    }
}

void apply_campaign_issues(FLevelScriptCatalogResult& result) {
    auto const levels{to_native_levels(result.entries)};
    auto const campaigns{to_native_campaigns(result.campaigns)};
    auto const issues{::ioj::levels::validate_campaign_catalog(campaigns, levels)};
    for (auto const& issue : issues) {
        auto& entry{result.campaigns[static_cast<int32>(issue.entry_index)]};
        auto const message{ml::to_fstring(issue.diagnostic.message)};
        append_error(entry.error, message);
        entry.definition.Reset();
        append_error(result.error, FString::Printf(TEXT("%s: %s"), *entry.filename, *message));
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

    FDefinitionReader reader{FPaths::Combine(result.directory, TEXT("Libraries"))};
    result.campaigns.Reserve(filenames.Num());
    for (auto const& filename : filenames) {
        auto const path{FPaths::Combine(campaign_directory, filename)};
        FCampaignScriptEntry entry{.filename = filename, .path = path};
        auto read_result{reader.read_campaign_file(path)};
        if (!read_result) {
            entry.error = ml::to_fstring(format_diagnostics(read_result.error()));
            append_error(result.error, FString::Printf(TEXT("%s: %s"), *filename, *entry.error));
        } else {
            entry.definition = MoveTemp(*read_result);
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

auto catalog_category(ml::FLevelDefinition const& definition) noexcept -> ELevelCatalogCategory {
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

    FDefinitionReader reader{FPaths::Combine(result.directory, TEXT("Libraries"))};
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

        auto read_result{reader.read_level_source(entry.source_text)};
        if (read_result) {
            entry.display_title = read_result->metadata.title;
            entry.description = read_result->metadata.description;
            entry.definition = MoveTemp(*read_result);
        } else {
            entry.error = ml::to_fstring(format_diagnostics(read_result.error()));
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
} // namespace ioj::levels::authoring
