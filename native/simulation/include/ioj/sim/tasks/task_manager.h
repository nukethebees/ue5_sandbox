#pragma once

#include <ioj/sim/tasks/instruction.h>
#include <ioj/sim/tasks/instruction_data.h>
#include <ioj/sim/tasks/program_handle.h>
#include <ioj/sim/tasks/programs.h>
#include <ioj/sim/tasks/task_allocation_context.h>
#include <ioj/sim/tasks/tasks.h>

#include <initializer_list>
#include <span>
#include <vector>

namespace ioj::sim::tasks {
class TaskManager {
  public:
    static constexpr ProgramHandle idle_program{0};

    TaskManager();

    // Invalidates custom program and task handles while retaining allocated storage.
    void reset();
    // Program sizes must be nonzero and fit InstructionCount.
    [[nodiscard]] auto add_program(std::span<Instruction const> program) -> ProgramHandle;
    [[nodiscard]] auto add_program(std::initializer_list<Instruction> program) -> ProgramHandle;

    void plan_tasks();
    void allocate(TaskAllocationContext ctx);

  private:
    std::vector<Instruction> instructions_;
    InstructionData instruction_data_;
    Programs programs_;
    Tasks tasks_;
};
}
