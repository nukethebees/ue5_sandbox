#include <SpaceGame/simulation/LevelPresentationConfig.h>

#include <SpaceGame/simulation/SpaceGameLevelConfig.h>

namespace ioj::presentation_settings {
template <typename T>
auto assign(settings::Setting const& setting, T& target) -> bool {
    if constexpr (std::same_as<T, FVector2D> || std::same_as<T, FVector3f>) {
        auto const* values{std::get_if<std::vector<double>>(&setting.value)};
        constexpr auto dimensions{std::same_as<T, FVector2D> ? 2u : 3u};
        if (!values || values->size() != dimensions) {
            return false;
        }

        target.X = static_cast<decltype(target.X)>((*values)[0]);
        target.Y = static_cast<decltype(target.Y)>((*values)[1]);
        if constexpr (dimensions == 3) {
            target.Z = static_cast<decltype(target.Z)>((*values)[2]);
        }
        return true;
    } else {
        return setting.assign(target);
    }
}

template <typename Apply>
void fields(USpaceGameLevelConfig& target, Apply&& apply) {
    apply("presentation.player-ship.boost-effect-colour-intensity",
          target.player_ship.boost_effect_colour_intensity);
    apply("presentation.player-ship.barrel-roll.roll-speed",
          target.player_ship.barrel_roll_config.roll_speed);
    apply("presentation.player-ship.barrel-roll.roll-duration",
          target.player_ship.barrel_roll_config.roll_duration);
    apply("presentation.player-ship.barrel-roll.roll-cooldown",
          target.player_ship.barrel_roll_config.roll_cooldown);
    apply("presentation.laser-projectiles.min-cull-distance",
          target.laser_projectiles.min_cull_distance);
    apply("presentation.laser-projectiles.max-cull-distance",
          target.laser_projectiles.max_cull_distance);
    apply("presentation.capital-ships.n-small-explosions", target.capital_ships.n_small_explosions);
    apply("presentation.capital-ships.time-between-explosions",
          target.capital_ships.time_between_explosions);
    apply("presentation.capital-ships.min-small-explosion-range",
          target.capital_ships.min_small_explosion_range);
    apply("presentation.capital-ships.max-small-explosion-range",
          target.capital_ships.max_small_explosion_range);
    apply("presentation.capital-ships.large-explosion-delay",
          target.capital_ships.large_explosion_delay);
    apply("presentation.capital-ships.proxy-arrow-size", target.capital_ships.proxy_arrow_size);
    apply("presentation.turrets.death-effect-scale", target.turrets.death_effect_scale);
    apply("presentation.entity-overlay.enabled", target.entity_overlay.enabled);
    apply("presentation.entity-overlay.maximum-range", target.entity_overlay.maximum_range);
    apply("presentation.entity-overlay.bar-size-pixels", target.entity_overlay.bar_size_pixels);
    apply("presentation.entity-overlay.screen-offset-pixels",
          target.entity_overlay.screen_offset_pixels);
    apply("presentation.entity-overlay.minimum-bar-scale", target.entity_overlay.minimum_bar_scale);
    apply("presentation.entity-overlay.maximum-bar-scale", target.entity_overlay.maximum_bar_scale);
    apply("presentation.entity-overlay.inset-pixels", target.entity_overlay.inset_pixels);
    apply("presentation.entity-overlay.maximum-inset-height-ratio",
          target.entity_overlay.maximum_inset_height_ratio);
    apply("presentation.entity-overlay.objective-frame-pixels",
          target.entity_overlay.objective_frame_pixels);
    apply("presentation.entity-overlay.objective-bar-height-scale",
          target.entity_overlay.objective_bar_height_scale);
    apply("presentation.entity-overlay.screen-edge-padding-pixels",
          target.entity_overlay.screen_edge_padding_pixels);
    apply("presentation.entity-overlay.soft-target.acquisition-radius-pixels",
          target.entity_overlay.soft_target.acquisition_radius_pixels);
    apply("presentation.entity-overlay.soft-target.retention-radius-pixels",
          target.entity_overlay.soft_target.retention_radius_pixels);
    apply("presentation.entity-overlay.soft-target.centre-tie-radius-pixels",
          target.entity_overlay.soft_target.centre_tie_radius_pixels);
    apply("presentation.entity-overlay.soft-target.switch-improvement-ratio",
          target.entity_overlay.soft_target.switch_improvement_ratio);
    apply("presentation.entity-overlay.soft-target.range-transition-start-multiplier",
          target.entity_overlay.soft_target.range_transition_start_multiplier);
    apply("presentation.entity-overlay.soft-target.minimum-radius-pixels",
          target.entity_overlay.soft_target.minimum_radius_pixels);
    apply("presentation.entity-overlay.soft-target.maximum-radius-pixels",
          target.entity_overlay.soft_target.maximum_radius_pixels);
    apply("presentation.entity-overlay.soft-target.bounds-padding-pixels",
          target.entity_overlay.soft_target.bounds_padding_pixels);
    apply("presentation.entity-overlay.soft-target.bracket-start-radius-multiplier",
          target.entity_overlay.soft_target.bracket_start_radius_multiplier);
    apply("presentation.entity-overlay.soft-target.opacity",
          target.entity_overlay.soft_target.opacity);
    apply("presentation.entity-overlay.soft-target.pulse-opacity-boost",
          target.entity_overlay.soft_target.pulse_opacity_boost);
    apply("presentation.entity-overlay.soft-target.pulse-duration",
          target.entity_overlay.soft_target.pulse_duration);
    apply("presentation.entity-overlay.soft-target.fade-out-duration",
          target.entity_overlay.soft_target.fade_out_duration);
    apply("presentation.radar.enabled", target.radar.enabled);
    apply("presentation.radar.combat-range", target.radar.combat_range);
    apply("presentation.radar.tactical-range", target.radar.tactical_range);
    apply("presentation.radar.maximum-range", target.radar.maximum_range);
    apply("presentation.radar.combat-display-radius", target.radar.combat_display_radius);
    apply("presentation.radar.grid-opacity", target.radar.grid_opacity);
    apply("presentation.radar.core-cell-radius", target.radar.core_cell_radius);
    apply("presentation.radar.combat-cell-radius", target.radar.combat_cell_radius);
    apply("presentation.radar.tactical-cell-radius", target.radar.tactical_cell_radius);
    apply("presentation.radar.strategic-cell-radius", target.radar.strategic_cell_radius);
    apply("presentation.radar.glyph-size-scale", target.radar.glyph_size_scale);
    apply("presentation.radar.objective-size-multiplier", target.radar.objective_size_multiplier);
    apply("presentation.radar.objective-ring-padding-pixels",
          target.radar.objective_ring_padding_pixels);
    apply("presentation.radar.objective-ring-thickness-pixels",
          target.radar.objective_ring_thickness_pixels);
}
}

auto apply_presentation_settings(ioj::settings::Settings const& settings,
                                 USpaceGameLevelConfig& target)
    -> std::expected<void, ioj::levels::Diagnostics> {
    for (auto const& setting : settings) {
        if (!setting.path.starts_with("presentation.")) {
            continue;
        }

        bool found{};
        bool valid{};
        ioj::presentation_settings::fields(target, [&](std::string_view const path, auto& field) {
            if (setting.path == path) {
                found = true;
                valid = ioj::presentation_settings::assign(setting, field);
            }
        });
        if (!found || !valid) {
            return std::unexpected{
                ioj::levels::Diagnostics{{found ? ioj::levels::DiagnosticCode::InvalidType
                                                : ioj::levels::DiagnosticCode::UnknownProperty,
                                          setting.path,
                                          found ? "Presentation setting has an incompatible value"
                                                : "Unknown presentation setting"}}};
        }
    }
    return {};
}
