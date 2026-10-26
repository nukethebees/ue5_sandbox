#include "SpaceGamePresentation/presentation/EntityOverlaySource.h"

#include "ioj/sim/entities/entity_type.h"
#include "ioj/sim/entities/health.h"
#include <ioj/column_math.h>
#include <ioj/sim/entities/entity_id.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include <algorithm>
#include <utility>

namespace {
auto inverse_maximum_health(::ioj::sim::EntityType const type,
                            FEntityOverlayHealthMaximums const& maximums) -> float {
    int32 maximum{0};
    switch (type) {
        case ::ioj::sim::EntityType::CapitalShip: {
            maximum = maximums.capital_ship;
            break;
        }
        case ::ioj::sim::EntityType::Fighter: {
            maximum = maximums.fighter;
            break;
        }
        case ::ioj::sim::EntityType::Turret: {
            maximum = maximums.turret;
            break;
        }
        default: {
            return 0.0f;
        }
    }
    return maximum > 0 ? 1.0f / static_cast<float>(maximum) : 0.0f;
}

auto team_colour(::ioj::sim::EntityType const type,
                 ETestTeam const team,
                 FEntityOverlayTeamColours const& colours) -> FLinearColor {
    switch (type) {
        case ::ioj::sim::EntityType::CapitalShip: {
            return colours.capital_ship[team];
        }
        case ::ioj::sim::EntityType::Fighter: {
            return colours.fighter[team];
        }
        case ::ioj::sim::EntityType::Turret: {
            return colours.turret[team];
        }
        default: {
            return FLinearColor::White;
        }
    }
}
}

auto select_soft_target(std::span<ioj::presentation::AgentDisplayBatch const> const batches,
                        ioj::sim::EntityTypeRadii const& entity_type_radii,
                        TConstArrayView<EEntityOverlayObjectiveRole> const objective_roles,
                        FSoftTargetSelectionContext const& context,
                        ioj::settings::SoftTarget const& settings,
                        ioj::sim::EntityId const current_target)
    -> ioj::presentation::SoftTargetResult {
    if (!context.view.is_valid()) {
        return {};
    }
    auto const& view{context.view};
    ioj::presentation::SoftTargetContext native{
        .view = {.camera_origin = ioj::Vector3f{view.camera_origin},
                 .minimum = ioj::Vector2f{FVector2f{view.view_rect.Min}},
                 .size = ioj::Vector2f{FVector2f{view.view_rect.Size()}}},
        .aim_origin = ioj::Vector3f{context.aim_origin},
        .aim_direction = ioj::Vector3f{context.aim_direction},
        .camera_right = ioj::Vector3f{context.camera_right},
        .camera_up = ioj::Vector3f{context.camera_up},
        .player_team = ioj::to_native(context.player_team),
        .effective_weapon_range = context.effective_weapon_range,
        .maximum_overlay_range = context.maximum_overlay_range};
    for (std::size_t row{}; row < 4; ++row) {
        std::copy_n(view.view_projection.M[row], 4, native.view.view_projection[row].begin());
    }
    return ioj::presentation::select_soft_target(
        batches,
        entity_type_radii,
        {objective_roles.GetData(), static_cast<std::size_t>(objective_roles.Num())},
        native,
        settings,
        current_target);
}

auto collect_entity_overlay_instances(
    std::span<::ioj::presentation::AgentDisplayBatch const> const batches,
    ::ioj::sim::EntityTypeRadii const& entity_type_radii,
    TConstArrayView<EEntityOverlayObjectiveRole> const objective_roles,
    FEntityOverlayTeamColours const& team_colours,
    FEntityOverlayHealthMaximums const& maximum_health,
    FVector3f const origin,
    float const maximum_range,
    TArray<FEntityOverlayInstance>& output_instances,
    FEntityOverlayCollector& collector) -> FEntityOverlayCollectionResult {
    TRACE_CPUPROFILER_EVENT_SCOPE(EntityOverlay::CollectAgentSource);
    auto const count{::ioj::presentation::display_entity_count(batches)};
    check(objective_roles.Num() == count);
    collector.begin(origin, FMath::Max(maximum_range, 0.0f), output_instances);

    output_instances.Reserve(count);
    int32 output_index{};
    for (auto const& batch : batches) {
        auto const batch_count{static_cast<int32>(batch.num())};
        auto const entity_type{batch.type};
        auto const inverse_health{inverse_maximum_health(entity_type, maximum_health)};
        if (inverse_health <= 0.0f) {
            output_index += batch_count;
            continue;
        }

        auto const world_radius{entity_type_radii[entity_type]};
        for (int32 index{}; index < batch_count; ++index, ++output_index) {
            auto const health{batch.health(index)};
            if (::ioj::sim::is_dead(health)) {
                continue;
            }

            auto const objective_role{objective_roles[output_index]};
            static_cast<void>(collector.try_add_colored(
                ioj::to_unreal(::ioj::vector_at(batch.locations, index)),
                static_cast<float>(health.value()) * inverse_health,
                world_radius,
                team_colour(entity_type, ioj::to_unreal(batch.team(index)), team_colours),
                objective_role,
                objective_role != EEntityOverlayObjectiveRole::None));
        }
    }

    return {.candidate_count = output_instances.Num(),
            .invalid_health_count = collector.invalid_health_count()};
}
