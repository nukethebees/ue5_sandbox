#include <SpaceGameS7/level_script_catalog.h>

#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/catalog_validation.h>
#include <SpaceGame/levels/native_level_definition_conversion.h>

#include <SandboxCoreEngine/strings.h>

#include <Containers/StringConv.h>
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

void sort_campaigns(FLevelScriptCatalogResult& result) {
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

auto default_level_script_root() -> FString {
    return FPaths::Combine(default_level_script_directory(), TEXT("catalog.scm"));
}

auto load_level_script_catalog(FStringView const root_path) -> FLevelScriptCatalogResult {
    FLevelScriptCatalogResult result;
    auto const full_path{FPaths::ConvertRelativePathToFull(FString{root_path})};
    result.directory = FPaths::GetPath(full_path);
    auto catalog{DefinitionReader{}.read_root_file(std::filesystem::path{*full_path})};
    if (!catalog) {
        result.error = ml::to_fstring(format_diagnostics(catalog.error()));
        return result;
    }

    for (auto& native : catalog->levels) {
        auto const path{ml::to_fstring(ioj::path_to_utf8(native.source_path))};
        FLevelScriptEntry entry{
            .filename = FPaths::GetCleanFilename(path),
            .path = path,
            .display_title = FPaths::GetBaseFilename(path),
        };
        auto const source_read{!path.IsEmpty() &&
                               FFileHelper::LoadFileToString(entry.source_text, *path)};
        if (!native.definition) {
            entry.error = ml::to_fstring(format_diagnostics(native.definition.error()));
        } else if (!source_read) {
            entry.error = FString::Printf(TEXT("Could not read level script '%s'."), *path);
        } else {
            entry.definition = to_unreal(std::move(*native.definition));
            entry.display_title = entry.definition->metadata.title;
            entry.description = entry.definition->metadata.description;
        }
        result.entries.Add(MoveTemp(entry));
    }
    for (auto& native : catalog->campaigns) {
        auto const path{ml::to_fstring(ioj::path_to_utf8(native.source_path))};
        FCampaignScriptEntry entry{.filename = FPaths::GetCleanFilename(path), .path = path};
        if (!native.definition) {
            entry.error = ml::to_fstring(format_diagnostics(native.definition.error()));
            append_error(result.error,
                         FString::Printf(TEXT("%s: %s"), *entry.filename, *entry.error));
        } else {
            auto& definition{*native.definition};
            ml::FCampaignDefinition converted;
            converted.id = ml::FCampaignId{FName{ml::to_fstring(definition.id.value)}};
            converted.title = ml::to_fstring(definition.title);
            for (auto const& id : definition.level_ids) {
                converted.level_ids.Add(ml::FLevelId{FName{ml::to_fstring(id.value)}});
            }
            entry.definition = MoveTemp(converted);
        }
        result.campaigns.Add(MoveTemp(entry));
    }

    apply_level_issues(result);
    apply_campaign_issues(result);
    sort_campaigns(result);
    return result;
}

auto load_level_script_catalog() -> FLevelScriptCatalogResult {
    return load_level_script_catalog(default_level_script_root());
}
} // namespace ioj::levels::authoring
