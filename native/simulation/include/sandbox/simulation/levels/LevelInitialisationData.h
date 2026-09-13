#pragma once
#include <cstdint>
#include <optional>

#include <sandbox/simulation/levels/LevelMissionInitialisationData.h>

namespace ml {
struct FLevelInitialisationData {
    std::optional<FLevelMissionInitialisationData> mission{};
    std::int32_t entity_count{};
    std::int32_t player_entity_index{-1};
};
}
