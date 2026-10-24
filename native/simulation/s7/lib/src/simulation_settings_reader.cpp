#include <ioj/s7/simulation_settings_reader.h>

#include "simulation_fields.h"
#include <ioj/levels/authoring/definition_reader.h>
#include <ioj/levels/level_definition.h>

#include <optional>
#include <utility>

namespace ioj::s7 {
namespace simulation_settings {
using sim::ShipFireRate;
using sim::ShipLaserMode;
using sim::SimulationSettings;
template <typename T>
auto assign(settings::Setting const& setting, T& target) -> std::optional<levels::DiagnosticCode> {
    if (!setting.assign(target)) {
        return levels::DiagnosticCode::InvalidType;
    }
    return std::nullopt;
}

template <typename T, typename Target>
auto assign_constrained(settings::Setting const& setting, Target& target)
    -> std::optional<levels::DiagnosticCode> {
    T value{};
    if (!setting.assign(value)) {
        return levels::DiagnosticCode::InvalidType;
    }

    auto const constrained{Target::try_make(value)};
    if (!constrained) {
        return levels::DiagnosticCode::InvalidNumber;
    }
    target = *constrained;
    return std::nullopt;
}

template <typename T, ConstrainedValuePolicy Policy>
auto assign(settings::Setting const& setting, ConstrainedValue<T, Policy>& target)
    -> std::optional<levels::DiagnosticCode> {
    return assign_constrained<T>(setting, target);
}

auto assign(settings::Setting const& setting, Frequency& target)
    -> std::optional<levels::DiagnosticCode> {
    return assign_constrained<double>(setting, target);
}

template <typename T, typename Parse>
auto assign_enum(settings::Setting const& setting, T& target, Parse parse)
    -> std::optional<levels::DiagnosticCode> {
    auto const* name{std::get_if<std::string>(&setting.value)};
    if (!name) {
        return levels::DiagnosticCode::InvalidType;
    }
    auto const value{parse(*name)};
    if (!value) {
        return levels::DiagnosticCode::InvalidType;
    }
    target = *value;
    return std::nullopt;
}

auto assign(settings::Setting const& setting, ShipLaserMode& target)
    -> std::optional<levels::DiagnosticCode> {
    return assign_enum(setting, target, sim::try_parse_serialized_ship_laser_mode);
}

auto assign(settings::Setting const& setting, ShipFireRate& target)
    -> std::optional<levels::DiagnosticCode> {
    return assign_enum(setting, target, sim::try_parse_serialized_ship_fire_rate);
}

auto apply_settings(settings::Settings const& settings, SimulationSettingsInput& target)
    -> std::expected<void, levels::Diagnostics> {
    levels::Diagnostics errors;
    for (auto const& setting : settings) {
        auto const field{simulation_fields::try_parse_fields_field(setting.path)};
        if (field) {
            std::optional<levels::DiagnosticCode> error;
            static_cast<void>(
                simulation_fields::visit_fields_field(target, *field, [&](auto& value) {
                    error = assign(setting, value);
                    return !error;
                }));
            if (error) {
                errors.emplace_back(*error,
                                    setting.path,
                                    *error == levels::DiagnosticCode::InvalidNumber
                                        ? "Expected a finite positive value"
                                        : "Setting has an incompatible value");
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

auto apply_settings(levels::LevelDefinition const& level, SimulationSettingsInput& target)
    -> std::expected<void, levels::Diagnostics> {
    auto applied{apply_settings(level.settings(), target)};
    if (!applied) {
        return applied;
    }

    if (auto const& grid{level.collision_grid()}; grid) {
        if (grid->level_size) {
            target.values.collision_grid_size = Vector3f{*grid->level_size};
        }
        if (grid->cell_size) {
            target.values.collision_cell_size = Vector3f{*grid->cell_size};
        }
    }
    return {};
}

}

auto resolve_simulation_settings(settings::Settings const& source,
                                 levels::LevelDefinition const* const level)
    -> SimulationSettingsReadResult {
    sim::SimulationSettings result{};
    SimulationSettingsInput input{result};
    auto applied{simulation_settings::apply_settings(source, input)};
    if (applied && level) {
        applied = simulation_settings::apply_settings(*level, input);
    }
    if (applied) {
        auto const& values{input.attack_distance_band};
        auto const band{sim::AttackDistanceBand::make(
            values.minimum_ratio, values.desired_ratio, values.maximum_ratio)};
        if (!band) {
            return std::unexpected{
                levels::Diagnostics{{levels::DiagnosticCode::InvalidNumber,
                                     "fighters.attack-distance-band",
                                     "Expected 0 <= minimum <= desired <= maximum <= 1"}}};
        }
        result.fighters.attack_distance_band = *band;
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
