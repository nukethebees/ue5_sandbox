#pragma once

#include <SpaceGame/levels/LevelMissionInitialisationData.h>

namespace ml {
struct SPACEGAME_API FLevelInitialisationData {
    TOptional<FLevelMissionInitialisationData> mission{NullOpt};
    int32 entity_count{};
    int32 player_entity_index{INDEX_NONE};
};
}
