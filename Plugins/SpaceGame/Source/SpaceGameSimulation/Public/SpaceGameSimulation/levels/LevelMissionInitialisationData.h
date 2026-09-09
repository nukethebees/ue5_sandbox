#pragma once

#include <CoreMinimal.h>
#include <SpaceGameSimulation/levels/LevelMissionMode.h>

namespace ml {
struct SPACEGAMESIMULATION_API FLevelMissionInitialisationData {
    TArray<int32> must_survive_entity_indices{};
    TArray<int32> required_kill_entity_indices{};
    TArray<int32> hero_entity_indices{};
    ELevelMissionMode mode{ELevelMissionMode::Unspecified};
    TOptional<float> time_limit_seconds{NullOpt};
    TOptional<int32> kill_count{NullOpt};
    FName level_id{NAME_None};
    FString level_title{};
};
}
