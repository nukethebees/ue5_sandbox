#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/team_id.h>

#include <unordered_map>
#include <unordered_set>

namespace ioj::levels::detail {
using IdSet = std::unordered_set<EntityId>;
struct EntityValidationState {
    IdSet ids{};
    std::unordered_map<EntityId, TeamId> teams_by_id{};
    std::unordered_map<EntityId, double> spawn_times_by_id{};
    bool player_found{false};
};
}
