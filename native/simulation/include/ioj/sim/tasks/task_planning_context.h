#pragma once

#include <ioj/levels/team.h>
#include <ioj/sim/missions/leaf_kind.h>

#include <span>

namespace ioj::sim {
class Mission;
}

namespace ioj::sim::tasks {
struct TaskPlanningContext {
    std::span<Team const> active_teams;
    Mission const& mission;
};

[[nodiscard]] constexpr auto supports_task(LeafKind const kind) noexcept -> bool {
    switch (kind) {
        case LeafKind::EntityDestroyed:
        case LeafKind::EntityReachedPosition:
        case LeafKind::EntityKillsReached:
            return true;

        default:
            return false;
    }
}
}
