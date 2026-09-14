#pragma once

#include <cstdint>
#include <ioj/sim/levels/level_mission_mode.h>
#include <optional>
#include <string>
#include <vector>

namespace ioj::sim {
struct LevelMissionInitialisationData {
    std::vector<std::int32_t> must_survive_entity_indices{};
    std::vector<std::int32_t> required_kill_entity_indices{};
    std::vector<std::int32_t> hero_entity_indices{};
    levels::LevelMissionMode mode{levels::LevelMissionMode::Unspecified};
    std::optional<float> time_limit_seconds{};
    std::optional<std::int32_t> kill_count{};
    std::string level_id{};
    std::string level_title{};
};
}
