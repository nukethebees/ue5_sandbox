#include <ioj/s7/simulation_settings_reader.h>

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_collision_grid_definition.h>

namespace ioj::s7 {
auto resolve_simulation_settings(settings::Settings const& source,
                                 levels::LevelCollisionGridDefinition const* grid)
    -> SimulationSettingsReadResult {
    sim::SimulationSettings result{
        .clock = source.tick,
        .player = source.player_ship,
        .lasers = source.laser_projectiles,
        .capital_ships = source.capital_ships,
        .fighters = source.fighters,
        .turrets = source.turrets,
        .spinners = source.tube_spinners,
        .overlap_response = source.collision_grid,
        .hud = source.hud,
    };
    if (grid) {
        if (grid->level_size) {
            result.overlap_response.level_size = Vector3f{*grid->level_size};
        }
        if (grid->cell_size) {
            result.overlap_response.cell_size = Vector3f{*grid->cell_size};
        }
    }
    if (auto valid{sim::validate_settings(result)}; !valid) {
        return std::unexpected{std::move(valid.error())};
    }
    return result;
}

auto read_simulation_settings(std::filesystem::path const& path,
                              std::filesystem::path const& library_root)
    -> SimulationSettingsReadResult {
    auto source{levels::authoring::DefinitionReader{library_root}.read_settings_file(path)};
    if (!source) {
        return std::unexpected{std::move(source.error())};
    }
    return resolve_simulation_settings(*source);
}
}
