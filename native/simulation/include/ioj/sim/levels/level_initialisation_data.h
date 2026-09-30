#pragma once
#include <ioj/sim/levels/level_entity_index.h>
#include <ioj/sim/levels/level_mission_initialisation_data.h>

#include <cstdint>
#include <optional>

namespace ioj::sim {
struct LevelInitialisationData {
    std::optional<LevelMissionInitialisationData> mission{};
    std::uint32_t entity_count{};
    std::uint32_t player_entity_index{invalid_level_entity_index};
};
}
