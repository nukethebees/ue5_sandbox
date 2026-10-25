#include "validation.h"
#include <ioj/levels/grid_dimensions.h>

namespace ioj::levels::detail {
void validate_collision_grid(sim::OverlapResponseConfig const& grid, Diagnostics& result) {
    auto const valid_level_size{is_positive_finite(grid.level_size)};
    if (!valid_level_size) {
        result.emplace_back(DiagnosticCode::InvalidLevelSize,
                            "level.settings.collision-grid.level-size",
                            "Collision-grid level size must be finite and greater than zero");
    }
    auto const valid_cell_size{is_positive_finite(grid.cell_size)};
    if (!valid_cell_size) {
        result.emplace_back(DiagnosticCode::InvalidGridCellSize,
                            "level.settings.collision-grid.cell-size",
                            "Collision-grid cell size must be finite and greater than zero");
    }
    if (valid_level_size && valid_cell_size) {
        auto const x{levels::grid_axis_count(grid.level_size.x(), grid.cell_size.x())};
        auto const y{levels::grid_axis_count(grid.level_size.y(), grid.cell_size.y())};
        auto const z{levels::grid_axis_count(grid.level_size.z(), grid.cell_size.z())};
        if (!levels::grid_cell_count_fits(x, y, z)) {
            result.emplace_back(
                DiagnosticCode::InvalidGridDimensions,
                "level.settings.collision-grid",
                "Collision-grid dimensions and cell count must fit in 32-bit integers");
        }
    }
}
}
