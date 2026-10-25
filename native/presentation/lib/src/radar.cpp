#include <ioj/presentation/radar.h>

#include <ioj/vector_value_math.h>

#include <algorithm>
#include <cmath>

namespace ioj::presentation {
namespace radar {
inline constexpr float hex_inradius{0.8660254f};

auto curve_parameter(ioj::settings::Radar const& settings) -> float {
    auto const normalized_combat{settings.combat_range / settings.maximum_range};
    if (normalized_combat >= 1.0f || settings.combat_display_radius <= normalized_combat) {
        return 0.0f;
    }
    return (settings.combat_display_radius - normalized_combat) /
           (normalized_combat * (1.0f - settings.combat_display_radius));
}

auto map_distance(float const normalized_distance, float const curve) -> float {
    return ((1.0f + curve) * normalized_distance) / (1.0f + curve * normalized_distance);
}

auto to_radar_display_radius(float const world_distance, ioj::settings::Radar const& settings)
    -> float {
    return map_distance(world_distance / settings.maximum_range, curve_parameter(settings));
}

auto to_radar_position(Vector3f const local_delta,
                       ioj::settings::Radar const& settings,
                       float const curve) -> Vector3f {
    Vector2f const planar{local_delta.x(), local_delta.y()};
    auto const planar_distance{size(planar)};
    Vector2f mapped_planar{Vector2f{}};
    if (planar_distance > 1.e-8f) {
        auto const direction{planar / planar_distance};
        auto const hex_denominator{
            std::max(std::abs(direction.x()) * hex_inradius + std::abs(direction.y()) * 0.5f,
                     std::abs(direction.y()))};
        auto const boundary_radius{hex_inradius / std::max(hex_denominator, 1.e-8f)};
        mapped_planar = direction * map_distance(planar_distance / settings.maximum_range, curve) *
                        boundary_radius;
    }
    auto const normalized_height{std::abs(local_delta.z()) / settings.maximum_range};
    auto const mapped_height{map_distance(normalized_height, curve)};
    return {mapped_planar.x(),
            mapped_planar.y(),
            local_delta.z() < 0.0f ? -mapped_height : mapped_height};
}

auto to_radar_position(Vector3f const local_delta, ioj::settings::Radar const& settings)
    -> Vector3f {
    return to_radar_position(local_delta, settings, curve_parameter(settings));
}
} // namespace radar

auto sanitize_radar_settings(ioj::settings::Radar settings) -> ioj::settings::Radar {
    settings.maximum_range = std::max(settings.maximum_range, 1.e-8f);
    settings.combat_range = std::clamp(settings.combat_range, 1.e-8f, settings.maximum_range);
    settings.tactical_range =
        std::clamp(settings.tactical_range, settings.combat_range, settings.maximum_range);
    auto const linear_combat_radius{settings.combat_range / settings.maximum_range};
    settings.combat_display_radius =
        linear_combat_radius >= 1.0f - 1.e-4f
            ? linear_combat_radius
            : std::clamp(settings.combat_display_radius, linear_combat_radius, 1.0f - 1.e-4f);
    settings.grid_opacity = std::clamp(settings.grid_opacity, 0.0f, 1.0f);
    settings.strategic_cell_radius = std::clamp(settings.strategic_cell_radius, 0.01f, 1.0f);
    settings.tactical_cell_radius =
        std::clamp(settings.tactical_cell_radius, settings.strategic_cell_radius, 1.0f);
    settings.combat_cell_radius =
        std::clamp(settings.combat_cell_radius, settings.tactical_cell_radius, 1.0f);
    settings.core_cell_radius =
        std::clamp(settings.core_cell_radius, settings.combat_cell_radius, 1.0f);
    settings.glyph_size_scale = std::clamp(settings.glyph_size_scale, 0.5f, 2.0f);
    settings.objective_size_multiplier = std::clamp(settings.objective_size_multiplier, 1.0f, 2.0f);
    settings.objective_ring_padding_pixels =
        std::clamp(settings.objective_ring_padding_pixels, 0.0f, 12.0f);
    settings.objective_ring_thickness_pixels =
        std::clamp(settings.objective_ring_thickness_pixels, 0.5f, 4.0f);
    return settings;
}

}
