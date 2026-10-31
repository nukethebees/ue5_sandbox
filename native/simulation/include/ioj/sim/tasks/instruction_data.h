#pragma once

#include <ioj/vector.h>

#include <memory_resource>
#include <vector>

namespace ioj::sim::tasks {
class InstructionData {
  public:
    explicit InstructionData(
        std::pmr::memory_resource& resource = *std::pmr::new_delete_resource()) noexcept
        : locations{&resource} {}

    std::pmr::vector<Vector3f> locations;
};
}
