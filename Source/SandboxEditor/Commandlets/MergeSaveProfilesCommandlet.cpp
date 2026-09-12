#include "SandboxEditor/Commandlets/MergeSaveProfilesCommandlet.h"

#include <SpaceGame/persistence/SaveProfileFileStorage.h>
#include <SpaceGame/persistence/SaveProfileManager.h>
#include <SpaceGame/persistence/SpaceSaveGame.h>

#include <HAL/FileManager.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/Parse.h>
#include <Misc/Paths.h>

namespace merge_save_profiles {
struct FMergedProfile {
    FSaveProfileMetadata metadata{};
    FSaveProfileResultsData results{};
};

bool records_equal(FScoreRecord const& left, FScoreRecord const& right) {
    return left.date == right.date && left.level_name == right.level_name &&
           left.mission_mode == right.mission_mode && left.end_state == right.end_state &&
           left.fail_reason == right.fail_reason && left.kills == right.kills &&
           left.time_seconds == right.time_seconds && left.target_kills == right.target_kills &&
           left.target_completion_time == right.target_completion_time;
}

bool contains_record(TConstArrayView<FScoreRecord> const records, FScoreRecord const& candidate) {
    return records.ContainsByPredicate(
        [&candidate](FScoreRecord const& record) { return records_equal(record, candidate); });
}

template <typename SaveType>
auto load_save(FString const& path) -> SaveType* {
    TArray<uint8> bytes{};
    if (!FFileHelper::LoadFileToArray(bytes, *path)) {
        return nullptr;
    }
    return Cast<SaveType>(UGameplayStatics::LoadGameFromMemory(bytes));
}

void merge_metadata(FSaveProfileMetadata& destination, FSaveProfileMetadata const& source) {
    if (destination.created_at == FDateTime{} ||
        (source.created_at != FDateTime{} && source.created_at < destination.created_at)) {
        destination.created_at = source.created_at;
    }
    if (source.last_played_at > destination.last_played_at) {
        destination.display_name = source.display_name;
    }
    destination.debug_settings.unlock_all_missions |= source.debug_settings.unlock_all_missions;
    destination.debug_settings.start_levels_paused |= source.debug_settings.start_levels_paused;
}

void update_metadata(FSaveProfileMetadata& metadata, TConstArrayView<FScoreRecord> const records) {
    metadata.last_played_at = {};
    metadata.total_simulation_duration_seconds = 0.f;
    metadata.total_kills = 0;
    metadata.outcome_count = records.Num();
    for (auto const& record : records) {
        metadata.last_played_at = FMath::Max(metadata.last_played_at, record.date);
        metadata.total_simulation_duration_seconds += record.time_seconds;
        metadata.total_kills += record.kills;
    }
}
}

UMergeSaveProfilesCommandlet::UMergeSaveProfilesCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
}

int32 UMergeSaveProfilesCommandlet::Main(FString const& parameters) {
    using namespace merge_save_profiles;
    FString source_roots_argument{};
    FString destination_root{};
    FString excluded_profiles_argument{};
    if (!FParse::Value(*parameters, TEXT("SourceRoots="), source_roots_argument, false) ||
        !FParse::Value(*parameters, TEXT("DestinationRoot="), destination_root, false)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("MergeSaveProfiles requires -SourceRoots=\"root;root\" and "
                    "-DestinationRoot=\"root\"."));
        return 1;
    }
    FParse::Value(*parameters, TEXT("ExcludeProfiles="), excluded_profiles_argument, false);

    TArray<FString> source_roots{};
    source_roots_argument.ParseIntoArray(source_roots, TEXT(";"), true);
    TArray<FString> excluded_profiles{};
    excluded_profiles_argument.ParseIntoArray(excluded_profiles, TEXT(","), true);
    destination_root = FPaths::ConvertRelativePathToFull(destination_root);
    if (source_roots.IsEmpty() || IFileManager::Get().DirectoryExists(*destination_root)) {
        UE_LOG(
            LogTemp, Error, TEXT("No source roots were supplied or destination already exists."));
        return 1;
    }

    TMap<FString, FMergedProfile> merged_profiles{};
    int32 source_profile_count{};
    int32 source_record_count{};
    for (auto source_root : source_roots) {
        source_root = FPaths::ConvertRelativePathToFull(source_root);
        auto const index_path{FPaths::Combine(source_root, TEXT("SpaceProfileIndex.sav"))};
        auto* const index_save{load_save<USpaceSaveProfileIndexSaveGame>(index_path)};
        if (!IsValid(index_save) ||
            index_save->data.save_version != FSaveProfileIndexData::current_save_version) {
            UE_LOG(LogTemp, Error, TEXT("Unsupported or malformed profile index: %s"), *index_path);
            return 1;
        }

        for (auto const& metadata : index_save->data.profiles) {
            if (excluded_profiles.Contains(metadata.profile_id)) {
                continue;
            }
            auto const results_path{FPaths::Combine(
                source_root,
                FString::Printf(TEXT("SpaceProfile_%s_Results.sav"), *metadata.profile_id))};
            auto* const results_save{load_save<USpaceSaveProfileResultsSaveGame>(results_path)};
            if (!IsValid(results_save) ||
                results_save->data.save_version != FSaveProfileResultsData::current_save_version) {
                UE_LOG(LogTemp,
                       Error,
                       TEXT("Unsupported or malformed profile results: %s"),
                       *results_path);
                return 1;
            }

            ++source_profile_count;
            source_record_count += results_save->data.score_records.Num();
            auto* merged{merged_profiles.Find(metadata.profile_id)};
            if (merged == nullptr) {
                merged = &merged_profiles.Add(metadata.profile_id,
                                              {.metadata = metadata, .results = {}});
            } else {
                merge_metadata(merged->metadata, metadata);
            }
            for (auto const& record : results_save->data.score_records) {
                if (!contains_record(merged->results.score_records, record)) {
                    merged->results.score_records.Add(record);
                }
            }
        }
    }
    if (merged_profiles.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("No profiles remained after exclusions."));
        return 1;
    }

    auto const staging_root{destination_root + TEXT(".importing.") +
                            FGuid::NewGuid().ToString(EGuidFormats::Digits)};
    auto no_legacy = [](TArray<FScoreRecord>&) {
        return ml::ioj::ESaveProfileLoadResult::not_found;
    };
    auto storage{ml::ioj::make_file_profile_storage(staging_root, no_legacy)};
    FSaveProfileIndexData merged_index{};
    TArray<FString> profile_ids{};
    merged_profiles.GetKeys(profile_ids);
    profile_ids.Sort();
    auto const default_profile_index{profile_ids.Find(TEXT("default_profile"))};
    if (default_profile_index != INDEX_NONE) {
        profile_ids.Swap(0, default_profile_index);
    }
    for (auto const& profile_id : profile_ids) {
        auto& profile{merged_profiles.FindChecked(profile_id)};
        update_metadata(profile.metadata, profile.results.score_records);
        if (!storage.save_results(profile_id, profile.results)) {
            UE_LOG(LogTemp, Error, TEXT("Failed to stage results for %s."), *profile_id);
            IFileManager::Get().DeleteDirectory(*staging_root, false, true);
            return 1;
        }
        merged_index.profiles.Add(profile.metadata);
    }
    merged_index.active_profile_id = profile_ids[0];
    if (!storage.save_index(merged_index)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to stage the merged profile index."));
        IFileManager::Get().DeleteDirectory(*staging_root, false, true);
        return 1;
    }

    ml::ioj::FSaveProfileManager validator{storage};
    if (!validator.initialise()) {
        UE_LOG(LogTemp, Error, TEXT("The staged save set failed validation."));
        IFileManager::Get().DeleteDirectory(*staging_root, false, true);
        return 1;
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(destination_root), true);
    if (!IFileManager::Get().Move(*destination_root, *staging_root, false, true, false, true)) {
        UE_LOG(LogTemp, Error, TEXT("Failed to publish the merged Shared save root."));
        IFileManager::Get().DeleteDirectory(*staging_root, false, true);
        return 1;
    }

    int32 merged_record_count{};
    for (auto const& entry : merged_profiles) {
        merged_record_count += entry.Value.results.score_records.Num();
    }
    UE_LOG(LogTemp,
           Display,
           TEXT("MERGE_SAVE_PROFILES_RESULT\nstatus=success\nsources=%d\nsource_profiles=%d\n"
                "profiles=%d\nsource_records=%d\nunique_records=%d\ndestination=%s"),
           source_roots.Num(),
           source_profile_count,
           merged_profiles.Num(),
           source_record_count,
           merged_record_count,
           *destination_root);
    return 0;
}
