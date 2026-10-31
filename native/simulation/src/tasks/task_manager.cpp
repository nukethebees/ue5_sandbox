#include <ioj/sim/tasks/task_manager.h>

#include <cassert>

namespace ioj::sim::tasks {
void TaskManager::reset() noexcept {
    instructions_.clear();
    instruction_data_.locations.clear();
    tasks_.base_instructions.clear();
    tasks_.instruction_counts.clear();
    for (auto const type : EnumTraits<EntityType>::values) {
        tasks_.entity_allocations[type].clear();
    }
}

auto TaskManager::add_program(std::span<Instruction const> const program) -> InstructionHandle {
    assert(!program.empty());
    auto const first{instructions_.size()};
    assert(first < InstructionHandle::invalid_value);
    assert(program.size() <= InstructionHandle::invalid_value - first);

    instructions_.insert(instructions_.end(), program.begin(), program.end());
    return InstructionHandle{static_cast<InstructionHandle::storage_type>(first)};
}

auto TaskManager::add_program(std::initializer_list<Instruction> const program)
    -> InstructionHandle {
    return add_program(std::span<Instruction const>{program.begin(), program.size()});
}

void TaskManager::plan_tasks() {}

void TaskManager::allocate(TaskAllocationContext) {}
}
