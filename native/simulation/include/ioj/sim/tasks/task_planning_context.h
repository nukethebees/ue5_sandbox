#pragma once

#include <span>

namespace ioj::sim {
struct EntityId;
class Mission;
}

namespace ioj::sim::tasks {
struct TaskPlanningContext {
    Mission const& mission;
    std::span<EntityId const> entities;
};

}
