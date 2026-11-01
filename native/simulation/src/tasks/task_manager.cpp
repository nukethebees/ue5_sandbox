#include <ioj/sim/tasks/task_manager.h>

#include <cassert>

namespace ioj::sim::tasks {
TaskManager::TaskManager(std::pmr::memory_resource& resource)
    : instructions_{&resource}
    , instruction_data_{resource}
    , programs_{resource}
    , tasks_{resource} {
    reset();
}

void TaskManager::reset() {
    instructions_.clear();
    instruction_data_.locations.clear();
    programs_.starts.clear();
    programs_.counts.clear();
    tasks_.programs.clear();
    for (auto const type : EnumTraits<EntityType>::values) {
        tasks_.entity_allocations[type].clear();
    }

    static_cast<void>(add_program({Instruction::make_data_idle()}));
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

void TaskManager::plan_tasks(TaskPlanningContext const ctx) {
    auto const team_count{ctx.active_teams.size()};
    for (std::size_t index{}; index < team_count; ++index) {
        [[maybe_unused]] auto const team{ctx.active_teams[index]};
    }
}

void TaskManager::allocate(TaskAllocationContext) {}
}
