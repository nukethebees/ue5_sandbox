#include <ioj/s7/simulation_settings_reader.h>

#include "generated/simulation_fields.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_definition.h>

#include <utility>

namespace ioj::s7 {
namespace simulation_settings {
using sim::ShipFireRate;
using sim::ShipLaserMode;
using sim::SimulationSettings;
template <typename T>
auto assign(settings::Setting const& setting, T& target) -> bool {
    return setting.assign(target);
}

template <typename T, typename Parse>
auto assign_enum(settings::Setting const& setting, T& target, Parse parse) -> bool {
    auto const* name{std::get_if<std::string>(&setting.value)};
    if (!name) {
        return false;
    }
    auto const value{parse(*name)};
    if (!value) {
        return false;
    }
    target = *value;
    return true;
}

auto assign(settings::Setting const& setting, ShipLaserMode& target) -> bool {
    return assign_enum(setting, target, sim::try_parse_serialized_ship_laser_mode);
}

auto assign(settings::Setting const& setting, ShipFireRate& target) -> bool {
    return assign_enum(setting, target, sim::try_parse_serialized_ship_fire_rate);
}

auto apply_settings(settings::Settings const& settings, SimulationSettings& target)
    -> std::expected<void, levels::Diagnostics> {
    levels::Diagnostics errors;
    for (auto const& setting : settings) {
        auto const field{simulation_fields::try_parse_fields_field(setting.path)};
        if (field) {
            auto const assigned{simulation_fields::visit_fields_field(
                target, *field, [&](auto& value) { return assign(setting, value); })};
            if (!assigned) {
                errors.emplace_back(levels::DiagnosticCode::InvalidType,
                                    setting.path,
                                    "Setting has an incompatible value");
            }
        } else {
            auto const section{std::string_view{setting.path}.substr(0, setting.path.find('.'))};
            if (section != "presentation" && section != "starfield") {
                errors.emplace_back(levels::DiagnosticCode::UnknownProperty,
                                    setting.path,
                                    "Unknown simulation setting");
            }
        }
    }
    if (!errors.empty()) {
        return std::unexpected{std::move(errors)};
    }
    return {};
}

auto apply_settings(levels::LevelDefinition const& level, SimulationSettings& target)
    -> std::expected<void, levels::Diagnostics> {
    auto applied{apply_settings(level.settings(), target)};
    if (!applied) {
        return applied;
    }

    if (auto const& grid{level.collision_grid()}; grid) {
        auto const vector = [](Vector3d const value) {
            return Vector3f{static_cast<float>(value.x()),
                            static_cast<float>(value.y()),
                            static_cast<float>(value.z())};
        };
        if (grid->level_size) {
            target.collision_grid_size = vector(*grid->level_size);
        }
        if (grid->cell_size) {
            target.collision_cell_size = vector(*grid->cell_size);
        }
    }
    return {};
}

}

auto resolve_simulation_settings(settings::Settings const& source,
                                 levels::LevelDefinition const* const level)
    -> SimulationSettingsReadResult {
    sim::SimulationSettings result{};
    auto applied{simulation_settings::apply_settings(source, result)};
    if (applied && level) {
        applied = simulation_settings::apply_settings(*level, result);
    }
    if (applied) {
        applied = sim::validate_settings(result);
    }
    if (!applied) {
        return std::unexpected{std::move(applied.error())};
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
