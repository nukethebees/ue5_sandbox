#include <ioj/sim/tasks/executor_table.h>

#include <ioj/sim/tasks/task_manager.h>
#include <ioj/subspan.h>

#include <cassert>

namespace ioj::sim::tasks {
ExecutorTable::ExecutorTable(std::pmr::memory_resource& resource)
    : current_tasks(entity_identity_capacity, &resource)
    , pending_tasks(entity_identity_capacity, &resource)
    , current_instructions(entity_identity_capacity, &resource) {}

auto ExecutorTable::subspan_for(EntityType const type) -> Subspan {
    return Subspan{entity_identity_offsets[type], entity_lifetime_capacities[type]};
}

void ExecutorTable::initialise_entities(EntityType const type,
                                        std::span<EntityId const> const ids) {
    auto const view{this->view(type)};
    for (auto const id : ids) {
        assert(id.is_valid() && id.entity_type() == type && id.index() < view.current_tasks.size());
        auto const index{id.index()};
        view.current_tasks[index] = TaskManager::idle_task;
        view.pending_tasks[index] = TaskId{};
        view.current_instructions[index] = TaskManager::idle_instruction;
    }
}
}
