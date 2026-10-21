#pragma once

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
struct SpatialQueryManager;
}

namespace ioj::sim::lasers {
struct ActionInputs {
    float dt;
    SpatialQueryManager const& spatial_query_manager;
    FrameMemoryResource& scratch_resource;
};
} // namespace ioj::sim::lasers
