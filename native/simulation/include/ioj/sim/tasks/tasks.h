#pragma once

#include <ioj/enum_array.h>
#include <ioj/sim/entities/entity_type.h>
#include <ioj/sim/tasks/program_handle.h>
#include <ioj/sim/tasks/task_entity_count.h>
#include <ioj/sim/tasks/task_id.h>

#include <vector>

namespace ioj::sim::tasks {
class Tasks {
  public:
    using Counts = std::vector<TaskEntityCount>;

    // Requires a registered program handle.
    [[nodiscard]] auto add_task(ProgramHandle program) -> TaskId;

    // Immutable
    std::vector<ProgramHandle> programs;

    // Mutable
    EnumArray<EntityType, Counts> entity_allocations;
};
}
