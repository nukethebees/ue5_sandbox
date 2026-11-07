#include <ioj/presentation/entity_overlay.h>

#include <ioj/presentation/entity_overlay_health_maximums.h>
#include <ioj/presentation/entity_overlay_team_colours.h>
#include <ioj/presentation/soft_target.h>

#include <algorithm>
#include <cassert>

namespace ioj::presentation {
auto sanitize_entity_overlay_settings(settings::EntityOverlay settings) -> settings::EntityOverlay {
    settings.minimum_bar_scale = std::max(settings.minimum_bar_scale, 0.01f);
    settings.maximum_bar_scale = std::max(settings.maximum_bar_scale, settings.minimum_bar_scale);
    settings.objective_bar_height_scale = std::clamp(settings.objective_bar_height_scale, 1.f, 2.f);
    normalize_soft_target_settings(settings.soft_target);
    return settings;
}

namespace entity_overlay_detail {
auto inverse_maximum_health(EntityType const type, EntityOverlayHealthMaximums const maximums)
    -> float {
    std::int32_t maximum{};
    switch (type) {
        case EntityType::CapitalShip: {
            maximum = maximums.capital_ship;
            break;
        }
        case EntityType::Fighter: {
            maximum = maximums.fighter;
            break;
        }
        case EntityType::Turret: {
            maximum = maximums.turret;
            break;
        }
        default: {
            return 0.0f;
        }
    }
    return maximum > 0 ? 1.0f / static_cast<float>(maximum) : 0.0f;
}

auto team_colour(EntityType const type, Team const team, EntityOverlayTeamColours const& colours)
    -> ui::Color4f {
    switch (type) {
        case EntityType::CapitalShip: {
            return colours.capital_ship[team];
        }
        case EntityType::Fighter: {
            return colours.fighter[team];
        }
        case EntityType::Turret: {
            return colours.turret[team];
        }
        default: {
            return {1.f, 1.f, 1.f, 1.f};
        }
    }
}
}

auto collect_entity_overlay_instances(
    std::span<AgentDisplayBatch const> const batches,
    sim::EntityTypeRadii const& entity_type_radii,
    std::span<ui::entity_overlay::ObjectiveRole const> const objective_roles,
    EntityOverlayTeamColours const& team_colours,
    EntityOverlayHealthMaximums const& maximum_health,
    Vector3f const origin,
    float const maximum_range,
    std::vector<ui::entity_overlay::Instance>& output_instances) -> EntityOverlayCollectionResult {
    auto const count{display_entity_count(batches)};
    assert(objective_roles.size() == count);
    output_instances.clear();
    output_instances.reserve(count);
    ui::entity_overlay::Collector collector;
    collector.begin(origin, std::max(maximum_range, 0.f));

    std::size_t output_index{};
    for (auto const& batch : batches) {
        auto const batch_count{batch.num()};
        auto const entity_type{batch.type};
        auto const inverse_health{
            entity_overlay_detail::inverse_maximum_health(entity_type, maximum_health)};
        if (inverse_health <= 0.0f) {
            output_index += batch_count;
            continue;
        }

        auto const world_radius{entity_type_radii[entity_type]};
        for (std::uint32_t index{}; index < batch_count; ++index, ++output_index) {
            auto const health{batch.health(index)};
            if (sim::is_dead(health)) {
                continue;
            }

            auto const objective_role{objective_roles[output_index]};
            auto const display_data{ui::entity_overlay::pack_display_data(
                objective_role,
                entity_overlay_detail::team_colour(entity_type, batch.team(index), team_colours))};
            auto const addition{
                collector.try_add(vector_at(batch.locations, index),
                                  static_cast<float>(health.value()) * inverse_health,
                                  world_radius,
                                  display_data,
                                  objective_role != ui::entity_overlay::ObjectiveRole::None)};
            if (addition) {
                output_instances.push_back(addition->instance);
                if (addition->swap_index != ui::entity_overlay::invalid_index) {
                    std::swap(output_instances[addition->swap_index], output_instances.back());
                }
            }
        }
    }
    return {.candidate_count = static_cast<std::int32_t>(output_instances.size()),
            .invalid_health_count = collector.invalid_health_count()};
}
}
