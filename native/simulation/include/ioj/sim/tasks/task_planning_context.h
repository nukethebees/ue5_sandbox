#pragma once

#include <span>

namespace ioj::sim {
struct EntityId;
class ExprManager;
}

namespace ioj::sim::tasks {
struct TaskPlanningContext {
    ExprManager const& expressions;
    std::span<EntityId const> entities;
};

}
