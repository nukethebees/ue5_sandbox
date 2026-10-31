#include <ioj/sim/tasks/task_manager.h>

#include <cassert>
#include <limits>

namespace ioj::sim::tasks {
TaskManager::TaskManager() {
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

auto TaskManager::add_program(std::span<Instruction const> const program) -> ProgramHandle {
    assert(!program.empty());
    assert(program.size() <= (std::numeric_limits<InstructionCount>::max)());

    auto const first{instructions_.size()};
    assert(first < InstructionHandle::invalid_value);
    assert(program.size() <= InstructionHandle::invalid_value - first);

    auto const index{programs_.starts.size()};
    assert(index < ProgramHandle::invalid_value);

    instructions_.insert(instructions_.end(), program.begin(), program.end());
    programs_.starts.push_back(
        InstructionHandle{static_cast<InstructionHandle::storage_type>(first)});
    programs_.counts.push_back(static_cast<InstructionCount>(program.size()));
    return ProgramHandle{static_cast<ProgramHandle::storage_type>(index)};
}

auto TaskManager::add_program(std::initializer_list<Instruction> const program) -> ProgramHandle {
    return add_program(std::span<Instruction const>{program.begin(), program.size()});
}

void TaskManager::plan_tasks() {}

void TaskManager::allocate(TaskAllocationContext) {}
}
