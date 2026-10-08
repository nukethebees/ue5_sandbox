#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/level_mission_mode.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::levels {
struct LevelMissionDefinition {
    LevelMissionMode mode{LevelMissionMode::Unspecified};
    std::optional<float> time_limit_seconds{};
    std::optional<std::int32_t> kill_count{};
    std::vector<EntityId> hero_entity_ids{};
    std::vector<EntityId> must_survive_entity_ids{};
    std::vector<EntityId> required_kill_entity_ids{};
};
}
