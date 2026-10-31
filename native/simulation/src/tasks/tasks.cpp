#include <ioj/sim/tasks/tasks.h>

#include <array>
#include <cassert>
#include <cstddef>
#include <utility>

namespace ioj::sim::tasks {
Tasks::Tasks(std::pmr::memory_resource& resource) noexcept
    : programs{&resource}
    , entity_allocations{[&resource]<std::size_t... Indices>(std::index_sequence<Indices...>) {
        return std::array<Counts, sizeof...(Indices)>{
            (static_cast<void>(Indices), Counts{&resource})...};
    }(std::make_index_sequence<enum_count<EntityType>()>{})} {}

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
