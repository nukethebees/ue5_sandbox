#pragma once

#include <ioj/levels/identifiers.h>
#include <ioj/levels/team.h>

#include <unordered_map>
#include <unordered_set>

namespace ioj::levels::detail {
using IdSet = std::unordered_set<EntityId>;
struct EntityFacts {
    Team team{};
    double spawn_time_seconds{};
};
struct EntityValidationState {
    std::unordered_map<EntityId, EntityFacts> by_id{};
    bool player_found{false};
};
}
