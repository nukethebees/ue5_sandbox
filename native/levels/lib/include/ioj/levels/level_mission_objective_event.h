#pragma once

#include <ioj/levels/identifiers.h>

#include <cstdint>
#include <vector>

namespace ioj::levels {
struct LevelMissionObjectiveEvent {
    double time_seconds{};
    std::vector<EntityId> must_survive_entity_ids{};
    std::vector<EntityId> required_kill_entity_ids{};
    std::int32_t kill_target_increase{};
};
}
