#include "SpaceGame/persistence/SaveProfileManager.h"

#include <Misc/Guid.h>

namespace ml::ioj {
namespace save_profile_manager {
FString const default_profile_id{TEXT("default_profile")};
FString const default_profile_name{TEXT("Default Profile")};
FString const test_profile_id{TEXT("test_profile")};
FString const test_profile_name{TEXT("Test Profile")};
}

FSaveProfileManager::FSaveProfileManager(FSaveProfileStorage storage)
    : storage_{MoveTemp(storage)} {}

bool FSaveProfileManager::initialise() {
    if (!storage_.load_index || !storage_.save_index || !storage_.load_results ||
        !storage_.save_results || !storage_.load_legacy_results) {
        return false;
    }

    FSaveProfileIndexData loaded_index{};
    auto const index_result{storage_.load_index(loaded_index)};
    if (index_result == ESaveProfileLoadResult::failed) {
        return false;
    }

    if (index_result == ESaveProfileLoadResult::not_found) {
        TArray<FScoreRecord> legacy_records{};
        auto const legacy_result{storage_.load_legacy_results(legacy_records)};
        if (legacy_result == ESaveProfileLoadResult::failed) {
            return false;
        }

        return create_initial_profile(legacy_records);
    }

    index_ = MoveTemp(loaded_index);
    if (index_.profiles.IsEmpty()) {
        return create_initial_profile({});
    }

    if (!find_profile(index_.active_profile_id)) {
        index_.active_profile_id = index_.profiles[0].profile_id;
        if (!storage_.save_index(index_)) {
            return false;
        }
    }

    auto const results_result{storage_.load_results(index_.active_profile_id, active_results_)};
    if (results_result == ESaveProfileLoadResult::failed) {
        return false;
    }
    if (results_result == ESaveProfileLoadResult::not_found) {
        active_results_ = {};
        if (!storage_.save_results(index_.active_profile_id, active_results_)) {
            return false;
        }
    }

    auto* const active_metadata{find_profile(index_.active_profile_id)};
    check(active_metadata);
    update_metadata(*active_metadata, active_results_.score_records);
    if (!storage_.save_index(index_)) {
        return false;
    }

    initialised_ = true;
    return true;
}

auto FSaveProfileManager::get_profiles() const -> TConstArrayView<FSaveProfileMetadata> {
    return index_.profiles;
}

auto FSaveProfileManager::get_active_profile_id() const -> FString const& {
    return index_.active_profile_id;
}

auto FSaveProfileManager::get_active_records() const -> TConstArrayView<FScoreRecord> {
    return active_results_.score_records;
}

auto FSaveProfileManager::create_profile(FString display_name) -> FCreateSaveProfileResponse {
    display_name.TrimStartAndEndInline();
    auto const validation{validate_profile_name(display_name, index_.profiles)};
    if (validation != ECreateSaveProfileResult::succeeded) {
        return {.result = validation};
    }

    auto const profile_id{FGuid::NewGuid().ToString(EGuidFormats::Digits)};
    auto metadata{make_metadata(profile_id, display_name, FDateTime::Now(), {})};
    if (!save_new_profile(MoveTemp(metadata), {}, false)) {
        return {.result = ECreateSaveProfileResult::persistence_failed};
    }

    return {.result = ECreateSaveProfileResult::succeeded, .profile_id = profile_id};
}

bool FSaveProfileManager::activate_profile(FString const& profile_id) {
    if (!initialised_ || profile_id == index_.active_profile_id) {
        return initialised_ && find_profile(profile_id) != nullptr;
    }
    if (!find_profile(profile_id)) {
        return false;
    }

    FSaveProfileResultsData loaded_results{};
    auto const result{storage_.load_results(profile_id, loaded_results)};
    if (result != ESaveProfileLoadResult::succeeded) {
        return false;
    }

    auto const previous_profile_id{index_.active_profile_id};
    index_.active_profile_id = profile_id;
    if (!storage_.save_index(index_)) {
        index_.active_profile_id = previous_profile_id;
        return false;
    }

    active_results_ = MoveTemp(loaded_results);
    return true;
}

bool FSaveProfileManager::load_profile_records(FString const& profile_id,
                                               TArray<FScoreRecord>& records) const {
    if (!initialised_ || !find_profile(profile_id)) {
        return false;
    }
    if (profile_id == index_.active_profile_id) {
        records = active_results_.score_records;
        return true;
    }

    FSaveProfileResultsData results{};
    if (storage_.load_results(profile_id, results) != ESaveProfileLoadResult::succeeded) {
        return false;
    }

    records = MoveTemp(results.score_records);
    return true;
}

bool FSaveProfileManager::append_score_record(FScoreRecord const& record) {
    if (!initialised_) {
        return false;
    }

    active_results_.score_records.Add(record);
    if (!storage_.save_results(index_.active_profile_id, active_results_)) {
        active_results_.score_records.Pop();
        return false;
    }

    auto* const metadata{find_profile(index_.active_profile_id)};
    check(metadata);
    auto const previous_metadata{*metadata};
    update_metadata(*metadata, active_results_.score_records);
    if (!storage_.save_index(index_)) {
        *metadata = previous_metadata;
        return false;
    }

    return true;
}

bool FSaveProfileManager::reset_test_profile(TConstArrayView<FScoreRecord> const records) {
    auto const created_at{records.IsEmpty() ? FDateTime::Now() : records[0].date};
    auto metadata{make_metadata(save_profile_manager::test_profile_id,
                                save_profile_manager::test_profile_name,
                                created_at,
                                records)};
    FSaveProfileResultsData results{};
    results.score_records.Append(records);
    return save_new_profile(MoveTemp(metadata), MoveTemp(results), true);
}

auto
    FSaveProfileManager::validate_profile_name(FString display_name,
                                               TConstArrayView<FSaveProfileMetadata> const profiles)
        -> ECreateSaveProfileResult {
    display_name.TrimStartAndEndInline();
    if (display_name.IsEmpty()) {
        return ECreateSaveProfileResult::empty_name;
    }
    if (display_name.Len() > max_profile_name_length) {
        return ECreateSaveProfileResult::name_too_long;
    }
    for (auto const& profile : profiles) {
        if (profile.display_name.Equals(display_name, ESearchCase::IgnoreCase)) {
            return ECreateSaveProfileResult::duplicate_name;
        }
    }
    return ECreateSaveProfileResult::succeeded;
}

auto FSaveProfileManager::make_metadata(FString profile_id,
                                        FString display_name,
                                        FDateTime const created_at,
                                        TConstArrayView<FScoreRecord> const records)
    -> FSaveProfileMetadata {
    FSaveProfileMetadata metadata{
        .profile_id = MoveTemp(profile_id),
        .display_name = MoveTemp(display_name),
        .created_at = created_at,
    };
    update_metadata(metadata, records);
    return metadata;
}

void FSaveProfileManager::update_metadata(FSaveProfileMetadata& metadata,
                                          TConstArrayView<FScoreRecord> const records) {
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

bool FSaveProfileManager::create_initial_profile(
    TConstArrayView<FScoreRecord> const legacy_records) {
    auto const created_at{legacy_records.IsEmpty() ? FDateTime::Now() : legacy_records[0].date};
    auto metadata{make_metadata(save_profile_manager::default_profile_id,
                                save_profile_manager::default_profile_name,
                                created_at,
                                legacy_records)};
    FSaveProfileResultsData results{};
    results.score_records.Append(legacy_records);
    return save_new_profile(MoveTemp(metadata), MoveTemp(results), false);
}

bool FSaveProfileManager::save_new_profile(FSaveProfileMetadata metadata,
                                           FSaveProfileResultsData results,
                                           bool const replace_existing) {
    if (!storage_.save_results(metadata.profile_id, results)) {
        return false;
    }

    auto const previous_index{index_};
    auto* existing{find_profile(metadata.profile_id)};
    if (existing) {
        if (!replace_existing) {
            return false;
        }
        *existing = metadata;
    } else {
        index_.profiles.Add(metadata);
    }
    index_.active_profile_id = metadata.profile_id;

    if (!storage_.save_index(index_)) {
        index_ = previous_index;
        return false;
    }

    active_results_ = MoveTemp(results);
    initialised_ = true;
    return true;
}

auto FSaveProfileManager::find_profile(FString const& profile_id) -> FSaveProfileMetadata* {
    return index_.profiles.FindByPredicate([&profile_id](FSaveProfileMetadata const& profile) {
        return profile.profile_id == profile_id;
    });
}

auto FSaveProfileManager::find_profile(FString const& profile_id) const
    -> FSaveProfileMetadata const* {
    return index_.profiles.FindByPredicate([&profile_id](FSaveProfileMetadata const& profile) {
        return profile.profile_id == profile_id;
    });
}
}
