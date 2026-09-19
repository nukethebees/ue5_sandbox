#pragma once

#include "ioj/sim/collision_grid.h"
#include "ioj/sim/world_aabbs.h"

#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <vector>

namespace ioj::sim::collision {
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
    using CellRangeIndex = std::int32_t;
    using AabbIndex = std::uint16_t;
    using RangeOffset = std::uint32_t;
    using RangeCount = std::uint16_t;

    inline static constexpr AabbIndex invalid_aabb_index{std::numeric_limits<AabbIndex>::max()};

    void reset() noexcept;
    void set_aabbs(WorldAABBs aabbs) noexcept;
    auto add_aabb(Vector3f min_point, Vector3f max_point) -> std::int32_t;
    [[nodiscard]] auto rebuild(GridGeometry geometry) -> std::expected<void, StaticGridBuildError>;

    [[nodiscard]] auto aabbs() const noexcept -> WorldAABBs const&;
    [[nodiscard]] auto aabb_indices_for_cell(std::int32_t cell_index) const noexcept
        -> std::span<AabbIndex const>;
  private:
    WorldAABBs aabbs_;
    std::vector<CellRangeIndex> cell_range_indices_;
    std::vector<RangeOffset> range_offsets_;
    std::vector<RangeCount> range_counts_;
    std::vector<AabbIndex> aabb_indices_;
};
} // namespace ioj::sim::collision
