#pragma once

#include <ioj/sim/tasks/instruction_count.h>
#include <ioj/sim/tasks/instruction_handle.h>

#include <memory_resource>
#include <vector>

namespace ioj::sim::tasks {
struct Programs {
    explicit Programs(
        std::pmr::memory_resource& resource = *std::pmr::new_delete_resource()) noexcept
        : starts{&resource}
        , counts{&resource} {}

    std::pmr::vector<InstructionHandle> starts;
    std::pmr::vector<InstructionCount> counts;
};
}
