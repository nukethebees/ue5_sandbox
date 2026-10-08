#include "validation.h"
#include <ioj/levels/grid_dimensions.h>

#include <cmath>
#include <format>

namespace ioj::levels::detail {
void validate_collision_grid(LevelCollisionGridDefinition const& grid, Diagnostics& result) {
    auto const valid_level_size{!grid.level_size || ml::is_positive_finite(*grid.level_size)};
    if (grid.level_size && !valid_level_size) {
        result.emplace_back(DiagnosticCode::InvalidLevelSize,
                            "level.collision-grid.level-size",
                            "Collision-grid level size must be finite and greater than zero");
    }
    auto const valid_cell_size{!grid.cell_size || ml::is_positive_finite(*grid.cell_size)};
    if (grid.cell_size && !valid_cell_size) {
        result.emplace_back(DiagnosticCode::InvalidGridCellSize,
                            "level.collision-grid.cell-size",
                            "Collision-grid cell size must be finite and greater than zero");
    }
    if (grid.level_size && grid.cell_size && valid_level_size && valid_cell_size) {
        auto const x{ioj::levels::grid_axis_count(static_cast<float>(grid.level_size->x),
                                                  static_cast<float>(grid.cell_size->x))};
        auto const y{ioj::levels::grid_axis_count(static_cast<float>(grid.level_size->y),
                                                  static_cast<float>(grid.cell_size->y))};
        auto const z{ioj::levels::grid_axis_count(static_cast<float>(grid.level_size->z),
                                                  static_cast<float>(grid.cell_size->z))};
        if (!ioj::levels::grid_cell_count_fits(x, y, z)) {
            result.emplace_back(
                DiagnosticCode::InvalidGridDimensions,
                "level.collision-grid",
                "Collision-grid dimensions and cell count must fit in 32-bit integers");
        }
    }
}
}
