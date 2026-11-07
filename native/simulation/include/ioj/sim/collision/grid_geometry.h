#pragma once

#include <ioj/math_types.h>
#include <ioj/sim/collision/cell_coord.h>
#include <ioj/sim/collision/collision_scalar_types.h>

#include <optional>

namespace ioj::sim::collision {
class GridGeometry {
  public:
    GridGeometry() noexcept = default;
    GridGeometry(CellCoord dimensions, Vector3f cell_dimensions);

    [[nodiscard]] static auto try_make(CellCoord dimensions, Vector3f cell_dimensions) noexcept
        -> std::optional<GridGeometry>;

    [[nodiscard]] auto dimensions() const noexcept -> CellCoord { return dimensions_; }
    [[nodiscard]] auto cell_dimensions() const noexcept -> Vector3f { return cell_dimensions_; }
    [[nodiscard]] auto half_size() const noexcept -> Vector3f { return half_size_; }
    [[nodiscard]] auto cell_count() const noexcept -> GridCellCount { return cell_count_; }

  private:
    GridGeometry(CellCoord dimensions,
                 Vector3f cell_dimensions,
                 Vector3f half_size,
                 GridCellCount cell_count) noexcept;

    CellCoord dimensions_{1, 1, 1};
    Vector3f cell_dimensions_{1.f, 1.f, 1.f};
    Vector3f half_size_{0.5f, 0.5f, 0.5f};
    GridCellCount cell_count_{1};
};
} // namespace ioj::sim::collision
