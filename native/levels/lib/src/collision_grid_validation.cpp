#include "validation.h"
#include <ioj/levels/grid_dimensions.h>

#include <cmath>
#include <format>

namespace ioj::levels::detail {
void validate_collision_grid(LevelCollisionGridDefinition const& grid, Diagnostics& result) {
    auto const valid_level_size{!grid.level_size ||
                                (std::isfinite(grid.level_size->x) &&
                                 std::isfinite(grid.level_size->y) &&
                                 std::isfinite(grid.level_size->z) && grid.level_size->x > 0.0 &&
                                 grid.level_size->y > 0.0 && grid.level_size->z > 0.0)};
    if (grid.level_size && !valid_level_size) {
        add_error(result,
                  "level.collision-grid.level-size",
                  DiagnosticCode::InvalidLevelSize,
                  "Collision-grid level size must be finite and greater than zero");
    }
    auto const valid_cell_size{
        !grid.cell_size || (std::isfinite(grid.cell_size->x) && std::isfinite(grid.cell_size->y) &&
                            std::isfinite(grid.cell_size->z) && grid.cell_size->x > 0.0 &&
                            grid.cell_size->y > 0.0 && grid.cell_size->z > 0.0)};
    if (grid.cell_size && !valid_cell_size) {
        add_error(result,
                  "level.collision-grid.cell-size",
                  DiagnosticCode::InvalidGridCellSize,
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
            add_error(result,
                      "level.collision-grid",
                      DiagnosticCode::InvalidGridDimensions,
                      "Collision-grid dimensions and cell count must fit in 32-bit integers");
        }
    }
}
}
