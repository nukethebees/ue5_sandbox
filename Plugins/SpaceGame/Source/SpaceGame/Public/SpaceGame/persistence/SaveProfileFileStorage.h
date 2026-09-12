#pragma once

#include "SpaceGame/persistence/SaveProfileManager.h"

namespace ml::ioj {
enum class ESaveProfileStorageMode : uint8 {
    platform,
    shared,
    local,
    experimental,
};

struct SPACEGAME_API FSaveProfileStorageSelection {
    ESaveProfileStorageMode mode{ESaveProfileStorageMode::platform};
    FString root{};
    FString experiment_name{};
    bool import_local{};
    bool start_fresh{};
    bool clone_shared{};
};

SPACEGAME_API auto development_save_base_root() -> FString;
SPACEGAME_API auto resolve_save_profile_storage(FString const& command_line,
                                                bool editor,
                                                bool unattended,
                                                bool automation,
                                                FSaveProfileStorageSelection& selection,
                                                FString& error) -> bool;

SPACEGAME_API auto make_file_profile_storage(
    FString root, TFunction<ESaveProfileLoadResult(TArray<FScoreRecord>&)> load_legacy_results)
    -> FSaveProfileStorage;

SPACEGAME_API auto clone_save_profile_files(FString const& source_root,
                                            FString const& destination_root,
                                            FString& error) -> bool;
SPACEGAME_API auto import_local_save_profiles(
    FString const& destination_root,
    TFunction<ESaveProfileLoadResult(TArray<FScoreRecord>&)> load_legacy_results,
    FString& error) -> bool;
}
