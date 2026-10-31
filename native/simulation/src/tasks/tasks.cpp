#include <ioj/sim/tasks/tasks.h>

#include <cassert>

namespace ioj::sim::tasks {
auto Tasks::add_task(ProgramHandle const program) -> TaskId {
    assert(program.is_valid());

    auto const index{programs.size()};
    assert(index < TaskId::invalid_value);

    programs.push_back(program);
    for (auto const type : EnumTraits<EntityType>::values) {
        entity_allocations[type].push_back(0);
    }
    return TaskId{static_cast<TaskId::storage_type>(index)};
}
}
