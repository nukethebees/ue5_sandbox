#pragma once

#include "ioj/sim/vectors3f.h"

#include <cstdint>
#include <limits>
#include <span>

namespace ioj::sim::collision {
using Vec3f = Vector3f;

inline constexpr float no_trace_hit{std::numeric_limits<float>::infinity()};

struct CellCoord {
    int x{};
    int y{};
    int z{};

    auto operator[](std::size_t index) noexcept -> int&;
    auto operator[](std::size_t index) const noexcept -> int;
    auto operator==(CellCoord const&) const noexcept -> bool = default;
};

struct CellCoordBounds {
    CellCoord min;
    CellCoord max;
};

struct GridGeometry {
    CellCoord dimensions;
    Vec3f cell_dimensions;
};

[[nodiscard]] auto is_configured(GridGeometry geometry) noexcept -> bool;
[[nodiscard]] auto num_cells(GridGeometry geometry) noexcept -> int;
[[nodiscard]] auto to_index(GridGeometry geometry, CellCoord coordinate) noexcept -> int;
[[nodiscard]] auto to_cell_coord(float value, float cell_dimension, int grid_dimension) noexcept
    -> int;
[[nodiscard]] auto to_cell_min(int coordinate, float cell_dimension, int grid_dimension) noexcept
    -> float;
[[nodiscard]] auto to_cell_coord(GridGeometry geometry, Vec3f position) noexcept -> CellCoord;
[[nodiscard]] auto to_max_cell_coord(GridGeometry geometry, Vec3f position) noexcept -> CellCoord;
[[nodiscard]] auto to_cell_coord_bounds(GridGeometry geometry,
                                        Vec3f min_point,
                                        Vec3f max_point) noexcept -> CellCoordBounds;
[[nodiscard]] auto to_cell_min(GridGeometry geometry, CellCoord coordinate) noexcept -> Vec3f;
[[nodiscard]] auto to_cell_centre(GridGeometry geometry, CellCoord coordinate) noexcept -> Vec3f;
[[nodiscard]] auto is_cell_coord_in_bounds(GridGeometry geometry, CellCoord coordinate) noexcept
    -> bool;
void are_spheres_in_bounds(GridGeometry geometry,
                           Vectors3fConstView centres,
                           float radius,
                           std::span<std::uint8_t> results) noexcept;

[[nodiscard]] auto trace_aabb(Vec3f trace_start,
                              Vec3f inverse_trace_delta,
                              Vec3f trace_delta,
                              Vec3f aabb_min,
                              Vec3f aabb_max,
                              Vec3f expansion = {}) noexcept -> float;

class GridTraversal {
  public:
    GridTraversal() noexcept = default;

    [[nodiscard]] static auto
        create(GridGeometry geometry, Vec3f start, Vec3f end, GridTraversal& result) noexcept
        -> bool;

    [[nodiscard]] auto current_cell() const noexcept -> CellCoord;
    [[nodiscard]] auto advance() noexcept -> bool;
  private:
    inline static constexpr float no_axis_crossing_t{std::numeric_limits<float>::infinity()};

    CellCoord current_cell_;
    CellCoord end_cell_;
    CellCoord steps_;
    Vec3f next_t_;
    Vec3f t_deltas_;
};
}
