#pragma once
#include <ioj/sim/levels/level_entity_index.h>

#include <cstdint>

namespace ioj::sim {
struct LevelInitialisationData {
    std::uint32_t entity_count{};
    LevelEntityIndex player_entity_index{invalid_level_entity_index};
};
}
