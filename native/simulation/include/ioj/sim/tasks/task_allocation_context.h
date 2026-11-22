#pragma once

#include <ioj/sim/entities/entity_id.h>
#include <ioj/sim/levels/level_read_access.h>

#include <memory_resource>
#include <span>

namespace ioj::sim {
class ConditionManager;
class LevelEventManager;
}

namespace ioj::sim::tasks {
struct ExecutorTable;

struct TaskAllocationContext {
    LevelReadAccess level;
    LevelEventManager const& events;
    ConditionManager const& conditions;
    std::span<EntityId const> entities;
    ExecutorTable& executors;
    std::pmr::memory_resource& scratch_resource;
};
}
