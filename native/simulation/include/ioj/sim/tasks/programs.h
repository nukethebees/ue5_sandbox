#pragma once

#include <ioj/sim/tasks/instruction_count.h>
#include <ioj/sim/tasks/instruction_handle.h>

#include <vector>

namespace ioj::sim::tasks {
struct Programs {
    std::vector<InstructionHandle> starts;
    std::vector<InstructionCount> counts;
};
}
