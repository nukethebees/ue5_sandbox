#pragma once

#include <ioj/sim/tasks/instruction.h>
#include <ioj/sim/tasks/instruction_data.h>
#include <ioj/sim/tasks/task_allocation_context.h>
#include <ioj/sim/tasks/tasks.h>

#include <vector>

namespace ioj::sim::tasks {
class TaskManager {
  public:
    void plan_tasks();
    void allocate(TaskAllocationContext ctx);

  private:
    std::vector<Instruction> instructions_;
    InstructionData instruction_data_;
    Tasks tasks_;
};
}
