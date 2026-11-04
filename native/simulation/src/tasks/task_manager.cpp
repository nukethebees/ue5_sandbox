#include <ioj/sim/tasks/task_manager.h>

#include <ioj/enum_array.h>
#include <ioj/require.h>

#include <array>
#include <cassert>
#include <utility>

namespace ioj::sim::tasks {
namespace task_manager_detail {
template <typename Enum, typename Factory>
auto make_array(Factory&& factory) {
    return [&factory]<std::size_t... Indices>(std::index_sequence<Indices...>) {
        using Value = decltype(factory());
        return EnumArray<Enum, Value>{
            std::array<Value, sizeof...(Indices)>{(static_cast<void>(Indices), factory())...}};
    }(std::make_index_sequence<enum_count<Enum>()>{});
}
}

[[nodiscard]] constexpr auto fits_instruction_range(std::size_t const first,
                                                    std::size_t const count) noexcept -> bool {
    return first < InstructionHandle::invalid_value &&
           count <= InstructionHandle::invalid_value - first;
}

[[nodiscard]] constexpr auto is_valid_task_index(std::size_t const index) noexcept -> bool {
    return index < TaskId::invalid_value;
}

TaskManager::TaskManager(std::pmr::memory_resource& resource)
    : instructions_{&resource}
    , instruction_data_{resource}
    , task_entries_{&resource}
    , active_tasks_{task_manager_detail::make_array<Team>(
          [&resource] { return std::pmr::vector<TaskId>{&resource}; })}
    , task_entity_allocations_{task_manager_detail::make_array<Team>([&resource] {
        return task_manager_detail::make_array<EntityType>(
            [&resource] { return Counts{&resource}; });
    })} {
    reset();
}

void TaskManager::reset() {
    instructions_.clear();
    instruction_data_.locations.clear();
    task_entries_.clear();
    for (auto& tasks : active_tasks_) {
        tasks.clear();
    }
    for (auto& team_allocations : task_entity_allocations_) {
        for (auto& counts : team_allocations) {
            counts.clear();
        }
    }

    static_cast<void>(add_task({Instruction::make_data_idle(), jump_to_idle}));
}

void TaskManager::reserve_instructions(std::size_t const count) {
    assert(fits_instruction_range(0, count));
    instructions_.reserve(count);
}

auto TaskManager::add_task(std::span<Instruction const> const instructions) -> TaskId {
    require(!instructions.empty() && instructions.back() == jump_to_idle,
            "Task instructions must end with Jump 0");

    auto const first{instructions_.size()};
    assert(fits_instruction_range(first, instructions.size()));

    auto const index{task_entries_.size()};
    assert(is_valid_task_index(index));

    instructions_.insert(instructions_.end(), instructions.begin(), instructions.end());
    task_entries_.push_back(InstructionHandle{static_cast<InstructionHandle::storage_type>(first)});
    for (auto& team_allocations : task_entity_allocations_) {
        for (auto& counts : team_allocations) {
            counts.push_back(0);
        }
    }
    return TaskId{static_cast<TaskId::storage_type>(index)};
}

auto TaskManager::add_task(std::initializer_list<Instruction> const instructions) -> TaskId {
    return add_task(std::span<Instruction const>{instructions.begin(), instructions.size()});
}

void TaskManager::plan_tasks(TaskPlanningContext const ctx) {
    auto const team_count{ctx.active_teams.size()};
    for (std::size_t index{}; index < team_count; ++index) {
        [[maybe_unused]] auto const team{ctx.active_teams[index]};
    }
}

void TaskManager::allocate(TaskAllocationContext) {}
}
