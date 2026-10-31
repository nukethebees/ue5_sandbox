#pragma once

#include <ioj/sim/tasks/instruction.h>
#include <ioj/sim/tasks/instruction_data.h>
#include <ioj/sim/tasks/instruction_handle.h>
#include <ioj/sim/tasks/task_allocation_context.h>
#include <ioj/sim/tasks/tasks.h>

#include <initializer_list>
#include <span>
#include <vector>

namespace ioj::sim::tasks {
class TaskManager {
  public:
    // Invalidates all program handles while retaining allocated storage.
    void reset() noexcept;
    // Programs must contain at least one instruction.
    [[nodiscard]] auto add_program(std::span<Instruction const> program) -> InstructionHandle;
    [[nodiscard]] auto add_program(std::initializer_list<Instruction> program) -> InstructionHandle;

    void plan_tasks();
    void allocate(TaskAllocationContext ctx);

  private:
    std::vector<Instruction> instructions_;
    InstructionData instruction_data_;
    Tasks tasks_;
};
}
