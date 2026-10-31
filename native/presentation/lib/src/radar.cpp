#include <ioj/presentation/radar.h>

#include <ioj/rotator3d.h>
#include <ioj/vector_value_math.h>

#include <algorithm>
#include <cassert>
#include <cmath>

namespace ioj::presentation {
namespace radar_detail {
auto glyph(EntityType const type) -> ui::radar::Glyph {
    switch (type) {
        case EntityType::PlayerShip: {
            return ui::radar::Glyph::Player;
        }
        case EntityType::Fighter: {
            return ui::radar::Glyph::Fighter;
        }
        case EntityType::CapitalShip: {
            return ui::radar::Glyph::CapitalShip;
        }
        case EntityType::Turret: {
            return ui::radar::Glyph::Turret;
        }
        default: {
            return ui::radar::Glyph::Unknown;
        }
    }
}

auto colour(Team const team, Team const player_team, RadarContactColours const& colours)
    -> ui::Color4f {
    if (team == Team::White) {
        return colours.neutral;
    }
    return team == player_team ? colours.friendly : colours.hostile;
}

auto size_scale(EntityType const type) -> float {
    switch (type) {
        case EntityType::CapitalShip: {
            return 1.1f;
        }
        case EntityType::Turret:
        case EntityType::Fighter: {
            return 1.0f;
        }
        default: {
            return 0.9f;
        }
    }
}

auto flags(ui::entity_overlay::ObjectiveRole const role) -> ui::radar::ContactFlags {
    if (role == ui::entity_overlay::ObjectiveRole::Defend) {
        return ui::radar::ContactFlags::DefendObjective;
    }
    if (role == ui::entity_overlay::ObjectiveRole::Destroy) {
        return ui::radar::ContactFlags::DestroyObjective;
    }
    return ui::radar::ContactFlags::None;
}
}

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

auto collect_radar_instances(
    std::span<AgentDisplayBatch const> const batches,
    std::span<ui::entity_overlay::ObjectiveRole const> const objective_roles,
    RadarContactColours const& contact_colours,
    Transform3d const& player_transform,
    sim::EntityId const player_id,
    Team const player_team,
    settings::Radar const& settings,
    RadarFrame& output_frame) -> RadarCollectionResult {
    auto const count{display_entity_count(batches)};
    assert(objective_roles.size() == count);
    output_frame.instances.clear();
    output_frame.instances.reserve(count + 1);

    auto rotation{to_rotator(player_transform.rotation)};
    rotation.roll = 0.0;
    auto const no_roll_rotation{to_quaternion(rotation)};
    auto const player_origin{Vector3f{player_transform.location}};
    auto const curve{radar::curve_parameter(settings)};
    output_frame.combat_display_radius = settings.combat_display_radius;
    output_frame.tactical_display_radius =
        radar::map_distance(settings.tactical_range / settings.maximum_range, curve);
    auto const range_squared{settings.maximum_range * settings.maximum_range};

    std::int32_t candidate_count{};
    for (auto const objectives : {false, true}) {
        std::size_t output_index{};
        for (auto const& batch : batches) {
            auto const batch_count{batch.num()};
            assert(batch.ids.size() == batch_count);
            auto const type{batch.type};
            if (type == EntityType::PlayerShip) {
                output_index += batch_count;
                continue;
            }

            auto const glyph{radar_detail::glyph(type)};
            auto const contact_size{radar_detail::size_scale(type)};
            for (std::uint32_t index{}; index < batch_count; ++index, ++output_index) {
                if (sim::is_dead(batch.health(index)) || batch.ids[index] == player_id) {
                    continue;
                }
                if (!objectives) {
                    ++candidate_count;
                }

                auto const flags{radar_detail::flags(objective_roles[output_index])};
                if ((flags != ui::radar::ContactFlags::None) != objectives) {
                    continue;
                }
                auto const world_delta{vector_at(batch.locations, index) - player_origin};
                if (size_squared(world_delta) >= range_squared) {
                    continue;
                }
                auto const local_delta{
                    Vector3f{unrotate_vector(no_roll_rotation, Vector3d{world_delta})}};
                float heading{};
                if (type == EntityType::Fighter) {
                    auto const local_velocity{
                        unrotate_vector(no_roll_rotation, Vector3d{batch.velocity(index)})};
                    if (local_velocity.x() * local_velocity.x() +
                            local_velocity.y() * local_velocity.y() >
                        1.e-8) {
                        heading =
                            static_cast<float>(std::atan2(local_velocity.y(), local_velocity.x()));
                    }
                }
                output_frame.instances.push_back(
                    {.radar_position = radar::to_radar_position(local_delta, settings, curve),
                     .size_scale = contact_size,
                     .packed_color = ui::radar::pack_color(
                         radar_detail::colour(batch.team(index), player_team, contact_colours)),
                     .packed_glyph_and_flags = ui::radar::pack_display(glyph, flags, heading)});
            }
        }
    }
    output_frame.instances.push_back(
        {.radar_position = {},
         .size_scale = 1.0f,
         .packed_color = ui::radar::pack_color({1.f, 0.72f, 0.18f, 1.f}),
         .packed_glyph_and_flags =
             ui::radar::pack_display(ui::radar::Glyph::Player, ui::radar::ContactFlags::None)});
    return {.candidate_count = candidate_count,
            .visible_count = static_cast<std::int32_t>(output_frame.instances.size())};
}

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
