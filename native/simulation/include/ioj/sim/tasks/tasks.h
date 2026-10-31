#pragma once

#include <ioj/enum_array.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/tasks/instruction_count.h>
#include <ioj/sim/tasks/instruction_handle.h>
#include <ioj/sim/tasks/task_entity_count.h>

#include <vector>

namespace ioj::sim::tasks {
class Tasks {
  public:
    using Counts = std::vector<TaskEntityCount>;

    // Immutable
    std::vector<InstructionHandle> base_instructions;
    std::vector<InstructionCount> instruction_counts;

    // Mutable
    EnumArray<EntityType, Counts> entity_allocations;
};
}
