#include <SpaceGameS7/level_script_catalog.h>

#include <ioj/files.h>
#include <ioj/levels/authoring/definition_reader.h>

#include <SandboxCoreEngine/strings.h>

#include <Misc/Paths.h>

namespace ioj::levels::authoring {
namespace {
void append_error(FString& errors, FString message) {
    if (!errors.IsEmpty()) {
        errors += TEXT("\n");
    }
    errors += MoveTemp(message);
}

} // namespace

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
        result.error = to_fstring(format_diagnostics(catalog.error()));
        return result;
    }

    for (auto const& campaign : catalog->campaigns) {
        if (!campaign.definition) {
            append_error(result.error, to_fstring(format_diagnostics(campaign.definition.error())));
        }
    }
    auto const issues{resolve_catalog(*catalog)};
    if (!issues.empty()) {
        append_error(result.error, to_fstring(format_diagnostics(issues)));
    }

    for (auto& native : catalog->levels) {
        auto const path{to_fstring(path_to_utf8(native.source_path))};
        FLevelScriptEntry entry{
            .filename = FPaths::GetCleanFilename(path),
            .path = path,
            .source_text = to_fstring(native.source_text),
            .display_title = FPaths::GetBaseFilename(path),
        };
        if (!native.definition) {
            entry.error = to_fstring(format_diagnostics(native.definition.error()));
        } else {
            entry.definition = std::move(*native.definition);
            entry.display_title = to_fstring(entry.definition->metadata().title);
            entry.description = to_fstring(entry.definition->metadata().description);
        }
        result.entries.Add(MoveTemp(entry));
    }
    for (auto& native : catalog->campaigns) {
        auto const path{to_fstring(path_to_utf8(native.source_path))};
        FCampaignScriptEntry entry{.filename = FPaths::GetCleanFilename(path), .path = path};
        if (!native.definition) {
            entry.error = to_fstring(format_diagnostics(native.definition.error()));
        } else {
            entry.definition = std::move(*native.definition);
        }
        result.campaigns.Add(MoveTemp(entry));
    }

    return result;
}

auto load_level_script_catalog() -> FLevelScriptCatalogResult {
    return load_level_script_catalog(default_level_script_root());
}
} // namespace ioj::levels::authoring
