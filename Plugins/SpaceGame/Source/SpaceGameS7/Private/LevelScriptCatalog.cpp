#include <SpaceGameS7/LevelScriptCatalog.h>

#include <SpaceGameS7/CampaignDefinitionReader.h>
#include <SpaceGameS7/LevelDefinitionReader.h>

#include <Containers/Map.h>
#include <Containers/Set.h>
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

auto valid_level_indices(TArray<FLevelScriptEntry> const& entries) -> TMap<FLevelId, int32> {
    TMap<FLevelId, int32> indices;
    indices.Reserve(entries.Num());
    auto const count{entries.Num()};
    for (int32 i{0}; i < count; ++i) {
        if (entries[i]) {
            indices.Add(entries[i].definition->metadata.id, i);
        }
    }
    return indices;
}

void invalidate_level(FLevelScriptCatalogResult& result, int32 const index, FString message) {
    auto& entry{result.entries[index]};
    entry.error = MoveTemp(message);
    entry.definition.Reset();
    append_error(result.error, FString::Printf(TEXT("%s: %s"), *entry.filename, *entry.error));
}

void invalidate_unavailable_unlock_references(FLevelScriptCatalogResult& result) {
    bool changed{true};
    while (changed) {
        changed = false;
        auto const indices{valid_level_indices(result.entries)};
        auto const count{result.entries.Num()};
        for (int32 i{0}; i < count; ++i) {
            auto const& entry{result.entries[i]};
            if (!entry) {
                continue;
            }
            for (auto const& criterion : entry.definition->unlock_criteria) {
                auto const target{criterion.Get<FLevelCompletedUnlockCriterion>().level_id};
                if (indices.Contains(target)) {
                    continue;
                }
                invalidate_level(
                    result,
                    i,
                    FString::Printf(TEXT("Level '%s' requires unavailable level '%s'."),
                                    *entry.definition->metadata.id.value.ToString(),
                                    *target.value.ToString()));
                changed = true;
                break;
            }
        }
    }
}

void invalidate_unlock_cycles(FLevelScriptCatalogResult& result) {
    auto const indices{valid_level_indices(result.entries)};
    TMap<FLevelId, uint8> visit_states;
    TArray<FLevelId> stack;
    TMap<FLevelId, FString> cycle_errors;

    TFunction<void(FLevelId)> visit = [&](FLevelId const id) {
        visit_states.Add(id, 1);
        stack.Add(id);

        auto const* const index{indices.Find(id)};
        check(index);
        auto const& definition{result.entries[*index].definition.GetValue()};
        for (auto const& criterion : definition.unlock_criteria) {
            auto const target{criterion.Get<FLevelCompletedUnlockCriterion>().level_id};
            auto const state{visit_states.FindRef(target)};
            if (state == 0) {
                visit(target);
                continue;
            }
            if (state != 1) {
                continue;
            }

            auto const cycle_start{stack.IndexOfByKey(target)};
            check(cycle_start != INDEX_NONE);
            TArray<FString> names;
            for (int32 i{cycle_start}; i < stack.Num(); ++i) {
                names.Add(stack[i].value.ToString());
            }
            names.Add(target.value.ToString());
            auto const message{FString::Printf(TEXT("Unlock dependency cycle: %s."),
                                               *FString::Join(names, TEXT(" -> ")))};
            for (int32 i{cycle_start}; i < stack.Num(); ++i) {
                cycle_errors.FindOrAdd(stack[i]) = message;
            }
        }

        stack.Pop(EAllowShrinking::No);
        visit_states[id] = 2;
    };

    for (auto const& pair : indices) {
        if (visit_states.FindRef(pair.Key) == 0) {
            visit(pair.Key);
        }
    }

    for (auto const& pair : cycle_errors) {
        invalidate_level(result, indices[pair.Key], pair.Value);
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

    FCampaignDefinitionReader reader;
    TMap<FCampaignId, int32> indices_by_id;
    indices_by_id.Reserve(filenames.Num());
    result.campaigns.Reserve(filenames.Num());
    for (auto const& filename : filenames) {
        auto const path{FPaths::Combine(campaign_directory, filename)};
        FCampaignScriptEntry entry{.filename = filename, .path = path};
        auto read_result{reader.read_file(path)};
        if (!read_result) {
            entry.error = format_read_error(read_result);
            append_error(result.error, FString::Printf(TEXT("%s: %s"), *filename, *entry.error));
            result.campaigns.Add(MoveTemp(entry));
            continue;
        }

        auto const id{read_result.definition->id};
        if (auto const* const existing_index{indices_by_id.Find(id)}) {
            auto& existing{result.campaigns[*existing_index]};
            auto const error{FString::Printf(TEXT("Campaign id '%s' is declared by both '%s' "
                                                  "and '%s'."),
                                             *id.value.ToString(),
                                             *existing.filename,
                                             *filename)};
            existing.error = error;
            existing.definition.Reset();
            entry.error = error;
            append_error(result.error, error);
            result.campaigns.Add(MoveTemp(entry));
            continue;
        }

        indices_by_id.Add(id, result.campaigns.Num());
        entry.definition = MoveTemp(read_result.definition);
        result.campaigns.Add(MoveTemp(entry));
    }

    auto const levels{valid_level_indices(result.entries)};
    for (auto& campaign : result.campaigns) {
        if (!campaign) {
            continue;
        }
        for (auto const level_id : campaign.definition->level_ids) {
            if (levels.Contains(level_id)) {
                continue;
            }
            campaign.error = FString::Printf(TEXT("Campaign '%s' references unavailable level "
                                                  "'%s'."),
                                             *campaign.definition->id.value.ToString(),
                                             *level_id.value.ToString());
            campaign.definition.Reset();
            append_error(result.error,
                         FString::Printf(TEXT("%s: %s"), *campaign.filename, *campaign.error));
            break;
        }
    }

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

    FLevelDefinitionReader reader;
    result.entries.Reserve(filenames.Num());
    TMap<FLevelId, int32> entry_indices_by_id;
    entry_indices_by_id.Reserve(filenames.Num());
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
            auto const id{read_result.definition->metadata.id};
            entry.display_title = read_result.definition->metadata.title;
            entry.description = read_result.definition->metadata.description;
            if (auto const* const existing_index{entry_indices_by_id.Find(id)}) {
                auto& existing_entry{result.entries[*existing_index]};
                auto const error{FString::Printf(TEXT("Level id '%s' is declared by both '%s' "
                                                      "and '%s'."),
                                                 *id.value.ToString(),
                                                 *existing_entry.filename,
                                                 *filename)};
                existing_entry.error = error;
                existing_entry.definition.Reset();
                entry.error = error;
                result.entries.Add(MoveTemp(entry));
                continue;
            }

            entry_indices_by_id.Add(id, result.entries.Num());
            entry.definition = MoveTemp(read_result.definition);
        } else {
            entry.error = format_read_error(read_result);
        }
        result.entries.Add(MoveTemp(entry));
    }

    invalidate_unavailable_unlock_references(result);
    invalidate_unlock_cycles(result);
    invalidate_unavailable_unlock_references(result);
    discover_campaigns(result);
    return result;
}

auto discover_level_scripts() -> FLevelScriptCatalogResult {
    return discover_level_scripts(default_level_script_directory());
}
}
