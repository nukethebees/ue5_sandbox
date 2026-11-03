#pragma once

#include <ioj/enum_array.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/tasks/instruction.h>
#include <ioj/sim/tasks/instruction_count.h>
#include <ioj/sim/tasks/instruction_data.h>
#include <ioj/sim/tasks/instruction_handle.h>
#include <ioj/sim/tasks/program_handle.h>
#include <ioj/sim/tasks/programs.h>
#include <ioj/sim/tasks/task_allocation_context.h>
#include <ioj/sim/tasks/task_entity_count.h>
#include <ioj/sim/tasks/task_id.h>
#include <ioj/sim/tasks/task_planning_context.h>

#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <memory_resource>
#include <span>
#include <vector>

namespace ioj::sim::tasks {
class TaskManager {
  public:
    using Counts = std::pmr::vector<TaskEntityCount>;

    static constexpr ProgramHandle idle_program{0};
    static constexpr TaskId idle_task{0};
    static constexpr InstructionHandle idle_instruction{0};

    // Keep the resource alive until after this manager and its storage are destroyed.
    explicit TaskManager(std::pmr::memory_resource& resource = *std::pmr::new_delete_resource());

    // Invalidates custom program and task handles while retaining allocated storage.
    void reset();
    void reserve_instructions(std::size_t count);
    // Program sizes must be nonzero and fit InstructionCount.
    [[nodiscard]] auto add_program(std::span<Instruction const> program) -> ProgramHandle;
    [[nodiscard]] auto add_program(std::initializer_list<Instruction> program) -> ProgramHandle;

    // Requires a registered program handle.
    [[nodiscard]] auto add_task(ProgramHandle program) -> TaskId;

    [[nodiscard]] auto instruction(InstructionHandle const handle) const -> Instruction {
        assert(handle.index() < instructions_.size());
        return instructions_[handle.index()];
    }
    [[nodiscard]] auto instruction_data() const noexcept -> InstructionData const& {
        return instruction_data_;
    }
    [[nodiscard]] auto first_instruction(TaskId const task) const -> InstructionHandle {
        assert(task.index() < task_programs_.size());
        auto const program{task_programs_[task.index()]};
        return programs_.starts[program.index()];
    }

    void plan_tasks(TaskPlanningContext ctx);
    void allocate(TaskAllocationContext ctx);

  private:
    std::pmr::vector<Instruction> instructions_;
    InstructionData instruction_data_;
    Programs programs_;

    // Immutable
    std::pmr::vector<ProgramHandle> task_programs_;

    // Mutable
    EnumArray<EntityType, Counts> task_entity_allocations_;
};
}
