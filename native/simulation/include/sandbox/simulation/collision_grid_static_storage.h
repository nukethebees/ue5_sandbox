#pragma once

#include "sandbox/simulation/collision_grid.h"
#include "sandbox/simulation/world_aabbs.h"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace ml::simulation::collision {
enum class StaticGridBuildErrorCode : std::uint8_t {
    AabbOutOfBounds,
    CellCountOverflow,
    MembershipCountOverflow,
    InconsistentWriteIndex,
};

struct StaticGridBuildError {
    StaticGridBuildErrorCode code;
    std::int32_t aabb_index{-1};
    std::int32_t cell_index{-1};
    std::int64_t count{};
};

class CollisionGridStaticStorage {
  public:
    void reset() noexcept;
    void set_aabbs(WorldAABBs aabbs) noexcept;
    auto add_aabb(Vector3f min_point, Vector3f max_point) -> std::int32_t;
    [[nodiscard]] auto rebuild(GridGeometry geometry) -> std::expected<void, StaticGridBuildError>;

    [[nodiscard]] auto aabbs() const noexcept -> WorldAABBs const&;
    [[nodiscard]] auto aabb_indices_for_cell(std::int32_t cell_index) const noexcept
        -> std::span<std::int32_t const>;
  private:
    WorldAABBs aabbs_;
    std::vector<std::int32_t> cell_range_indices_;
    std::vector<std::uint32_t> range_offsets_;
    std::vector<std::uint16_t> range_counts_;
    std::vector<std::int32_t> aabb_indices_;
};
} // namespace ml::simulation::collision
