#include <ioj/sim/tasks/task_manager.h>

#include <array>
#include <cassert>
#include <limits>
#include <utility>

namespace ioj::sim::tasks {
[[nodiscard]] constexpr auto is_valid_program_size(std::size_t const count) noexcept -> bool {
    return count != 0 && count <= (std::numeric_limits<InstructionCount>::max)();
}

[[nodiscard]] constexpr auto fits_instruction_range(std::size_t const first,
                                                    std::size_t const count) noexcept -> bool {
    return first < InstructionHandle::invalid_value &&
           count <= InstructionHandle::invalid_value - first;
}

[[nodiscard]] constexpr auto is_valid_program_index(std::size_t const index) noexcept -> bool {
    return index < ProgramHandle::invalid_value;
}

TaskManager::TaskManager(std::pmr::memory_resource& resource)
    : instructions_{&resource}
    , instruction_data_{resource}
    , programs_{resource}
    , task_programs_{&resource}
    , task_entity_allocations_{
          [&resource]<std::size_t... Indices>(std::index_sequence<Indices...>) {
              return std::array<Counts, sizeof...(Indices)>{
                  (static_cast<void>(Indices), Counts{&resource})...};
          }(std::make_index_sequence<enum_count<EntityType>()>{})} {
    reset();
}

void TaskManager::reset() {
    instructions_.clear();
    instruction_data_.locations.clear();
    programs_.starts.clear();
    programs_.counts.clear();
    task_programs_.clear();
    for (auto const type : EnumTraits<EntityType>::values) {
        task_entity_allocations_[type].clear();
    }

    static_cast<void>(add_program({Instruction::make_data_idle()}));
    static_cast<void>(add_task(idle_program));
}

void TaskManager::reserve_instructions(std::size_t const count) {
    assert(fits_instruction_range(0, count));
    instructions_.reserve(count);
}

auto TaskManager::add_program(std::span<Instruction const> const program) -> ProgramHandle {
    assert(is_valid_program_size(program.size()));

    auto const first{instructions_.size()};
    assert(fits_instruction_range(first, program.size()));

    auto const index{programs_.starts.size()};
    assert(is_valid_program_index(index));

    instructions_.insert(instructions_.end(), program.begin(), program.end());
    programs_.starts.push_back(
        InstructionHandle{static_cast<InstructionHandle::storage_type>(first)});
    programs_.counts.push_back(static_cast<InstructionCount>(program.size()));
    return ProgramHandle{static_cast<ProgramHandle::storage_type>(index)};
}

auto TaskManager::add_program(std::initializer_list<Instruction> const program) -> ProgramHandle {
    return add_program(std::span<Instruction const>{program.begin(), program.size()});
}

auto TaskManager::add_task(ProgramHandle const program) -> TaskId {
    assert(program.index() < programs_.starts.size());

    auto const index{task_programs_.size()};
    assert(index < TaskId::invalid_value);

    task_programs_.push_back(program);
    for (auto const type : EnumTraits<EntityType>::values) {
        task_entity_allocations_[type].push_back(0);
    }
    return TaskId{static_cast<TaskId::storage_type>(index)};
}

void TaskManager::plan_tasks(TaskPlanningContext const ctx) {
    auto const team_count{ctx.active_teams.size()};
    for (std::size_t index{}; index < team_count; ++index) {
        [[maybe_unused]] auto const team{ctx.active_teams[index]};
    }
}

void TaskManager::allocate(TaskAllocationContext) {}
}
