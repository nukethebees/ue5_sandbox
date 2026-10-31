#include <ioj/sim/tasks/executor_table.h>

#include <ioj/subspan.h>

#include <cassert>

namespace ioj::sim::tasks {
namespace executor_storage {
inline constexpr EntityTypeSizes offsets{[] {
    EntityTypeSizes result;
    std::uint32_t offset{};
    for (auto const type : EnumTraits<EntityType>::values) {
        result[type] = offset;
        offset += entity_lifetime_capacities[type];
    }
    return result;
}()};
}

ExecutorTable::ExecutorTable(std::pmr::memory_resource& resource)
    : current_tasks(entity_identity_capacity, &resource)
    , pending_tasks(entity_identity_capacity, &resource)
    , current_instructions(entity_identity_capacity, &resource) {}

auto ExecutorTable::subspan_for(EntityType const type) -> Subspan {
    return Subspan{executor_storage::offsets[type], entity_lifetime_capacities[type]};
}

void ExecutorTable::initialise_entities(EntityType const type,
                                        std::span<EntityId const> const ids) {
    auto const view{get_view(type)};
    for (auto const id : ids) {
        assert(id.is_valid() && id.entity_type() == type && id.index() < view.current_tasks.size());
        auto const index{id.index()};
        view.current_tasks[index] = TaskId{};
        view.pending_tasks[index] = TaskId{};
        view.current_instructions[index] = InstructionHandle{};
    }
}
}
