#pragma once

#include <ioj/sim/collision/cell_coord.h>
#include <ioj/sim/collision/cell_coord_bounds.h>
#include <ioj/sim/collision/grid_geometry.h>
#include <ioj/vector_types.h>

#include <limits>

namespace ioj::sim::collision {
class GridTraversal {
  public:
    GridTraversal() noexcept = default;
    [[nodiscard]] static auto
        create(GridGeometry geometry, Vector3f start, Vector3f end, GridTraversal& result) noexcept
        -> bool;
    [[nodiscard]] auto current_cell() const noexcept -> CellCoord;
    [[nodiscard]] auto advance() noexcept -> bool;
  private:
    inline static constexpr float no_axis_crossing_t{std::numeric_limits<float>::infinity()};

    CellCoord current_cell_;
    CellCoord end_cell_;
    CellCoord steps_;
    Vector3f next_t_;
    Vector3f t_deltas_;
};
}
