#pragma once

#include <cstdint>
#include <optional>
#include <sandbox/simulation/levels/LevelMissionMode.h>
#include <string>
#include <vector>

namespace ml {
struct FLevelMissionInitialisationData {
    std::vector<std::int32_t> must_survive_entity_indices{};
    std::vector<std::int32_t> required_kill_entity_indices{};
    std::vector<std::int32_t> hero_entity_indices{};
    ELevelMissionMode mode{ELevelMissionMode::Unspecified};
    std::optional<float> time_limit_seconds{};
    std::optional<std::int32_t> kill_count{};
    std::string level_id{};
    std::string level_title{};
};
}
