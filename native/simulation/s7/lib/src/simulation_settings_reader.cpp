#include <ioj/s7/simulation_settings_reader.h>

#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_definition.h>

namespace ioj::s7 {
namespace simulation_settings {
void apply(settings::Settings const& source, sim::SimulationSettings& target) {
    auto const group{[](auto& destination, auto const& patch) {
        if (patch) {
            apply_overrides(destination, *patch);
        }
    }};
    group(target.clock, source.tick);
    group(target.player, source.player_ship);
    group(target.lasers, source.laser_projectiles);
    group(target.capital_ships, source.capital_ships);
    group(target.fighters, source.fighters);
    group(target.turrets, source.turrets);
    group(target.spinners, source.tube_spinners);
    group(target.overlap_response, source.collision_grid);
    group(target.hud, source.hud);
}
}

auto resolve_simulation_settings(settings::Settings const& source,
                                 levels::LevelDefinition const* level)
    -> SimulationSettingsReadResult {
    sim::SimulationSettings result{};
    simulation_settings::apply(source, result);
    if (level) {
        simulation_settings::apply(level->settings(), result);
        if (auto const& grid{level->collision_grid()}; grid) {
            if (grid->level_size) {
                result.overlap_response.level_size = Vector3f{*grid->level_size};
            }
            if (grid->cell_size) {
                result.overlap_response.cell_size = Vector3f{*grid->cell_size};
            }
        }
    }
    if (auto valid{sim::validate_settings(result)}; !valid) {
        return std::unexpected{std::move(valid.error())};
    }
    return result;
}

auto read_simulation_settings(std::filesystem::path const& path,
                              std::filesystem::path const& library_root,
                              levels::LevelDefinition const* const level)
    -> SimulationSettingsReadResult {
    auto source{levels::authoring::DefinitionReader{library_root}.read_settings_file(path)};
    if (!source) {
        return std::unexpected{std::move(source.error())};
    }
    return resolve_simulation_settings(*source, level);
}
}
