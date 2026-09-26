#pragma once
#include <ioj/sim/levels/level_mission_initialisation_data.h>

#include <cstdint>
#include <optional>

namespace ioj::sim {
struct LevelInitialisationData {
    std::optional<LevelMissionInitialisationData> mission{};
    std::int32_t entity_count{};
    std::int32_t player_entity_index{-1};
};
}
