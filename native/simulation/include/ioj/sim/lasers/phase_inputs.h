#pragma once

#include <cstdint>
#include <memory_resource>
#include <vector>

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
struct SpatialQueryManager;
}

namespace ioj::sim::lasers {
using LaserIndex = std::uint32_t;

struct ActionInputs {
    float dt;
    SpatialQueryManager const& spatial_query_manager;
    FrameMemoryResource& scratch_resource;
    std::pmr::vector<LaserIndex>& pending_removals;
};

struct ResolutionCommitInputs {
    std::pmr::vector<LaserIndex>& pending_removals;
};
} // namespace ioj::sim::lasers
