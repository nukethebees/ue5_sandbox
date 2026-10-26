#include "SpaceGamePresentation/presentation/RadarSource.h"

#include "ioj/sim/entities/entity_type.h"
#include "ioj/sim/entities/health.h"
#include <ioj/column_math.h>
#include <ioj/sim/entities/entity_id.h>
#include <SpaceGamePresentation/entities/TestTeamConversion.h>

#include <SandboxCoreEngine/VectorConversion.h>

namespace ioj::radar_source {

auto glyph(sim::EntityType const type) -> ERadarGlyph {
    switch (type) {
        case ::ioj::sim::EntityType::PlayerShip: {
            return ERadarGlyph::Player;
        }
        case ::ioj::sim::EntityType::Fighter: {
            return ERadarGlyph::Fighter;
        }
        case ::ioj::sim::EntityType::CapitalShip: {
            return ERadarGlyph::CapitalShip;
        }
        case ::ioj::sim::EntityType::Turret: {
            return ERadarGlyph::Turret;
        }
        default: {
            return ERadarGlyph::Unknown;
        }
    }
}

auto colour(ETestTeam const team, ETestTeam const player_team, FRadarContactColours const& colours)
    -> FLinearColor {
    if (team == ETestTeam::White) {
        return colours.neutral;
    }
    return team == player_team ? colours.friendly : colours.hostile;
}

auto size_scale(sim::EntityType const type) -> float {
    switch (type) {
        case ::ioj::sim::EntityType::CapitalShip: {
            return 1.1f;
        }
        case ::ioj::sim::EntityType::Turret: {
            return 1.0f;
        }
        case ::ioj::sim::EntityType::Fighter: {
            return 1.0f;
        }
        default: {
            return 0.9f;
        }
    }
}

auto flags(EEntityOverlayObjectiveRole const objective_role) -> ERadarContactFlags {
    auto result{ERadarContactFlags::None};
    if (objective_role == EEntityOverlayObjectiveRole::Defend) {
        result |= ERadarContactFlags::DefendObjective;
    } else if (objective_role == EEntityOverlayObjectiveRole::Destroy) {
        result |= ERadarContactFlags::DestroyObjective;
    }
    return result;
}

auto draw_priority(ERadarContactFlags const contact_flags) -> int32 {
    if (EnumHasAnyFlags(contact_flags,
                        ERadarContactFlags::DefendObjective |
                            ERadarContactFlags::DestroyObjective)) {
        return 1;
    }
    return 0;
}

} // namespace ioj::radar_source

auto collect_radar_instances(std::span<::ioj::presentation::AgentDisplayBatch const> const batches,
                             TConstArrayView<EEntityOverlayObjectiveRole> const objective_roles,
                             FRadarContactColours const& contact_colours,
                             FTransform const& player_transform,
                             ::ioj::sim::EntityId const player_id,
                             ETestTeam const player_team,
                             ioj::settings::Radar const& settings,
                             FRadarFrame& output_frame) -> FRadarCollectionResult {
    TRACE_CPUPROFILER_EVENT_SCOPE(Radar::CollectAgentSource);
    auto const count{::ioj::presentation::display_entity_count(batches)};
    check(objective_roles.Num() == count);
    output_frame.instances.Reset();

    auto const rotation{player_transform.Rotator()};
    FTransform const no_roll_transform{FRotator{rotation.Pitch, rotation.Yaw, 0.0f},
                                       player_transform.GetLocation()};
    auto const player_origin{FVector3f{player_transform.GetLocation()}};
    auto const curve{ioj::presentation::radar::curve_parameter(settings)};
    output_frame.combat_display_radius = settings.combat_display_radius;
    output_frame.tactical_display_radius = ioj::presentation::radar::map_distance(
        settings.tactical_range / settings.maximum_range, curve);
    auto const range_squared{settings.maximum_range * settings.maximum_range};
    output_frame.instances.Reserve(count + 1);

    int32 candidate_count{0};
    for (int32 priority{0}; priority < 2; ++priority) {
        int32 output_index{};
        for (auto const& batch : batches) {
            auto const batch_count{static_cast<int32>(batch.num())};
            check(batch.ids.size() == static_cast<std::size_t>(batch_count));
            auto const entity_type{batch.type};
            if (entity_type == ::ioj::sim::EntityType::PlayerShip) {
                output_index += batch_count;
                continue;
            }

            auto const contact_glyph{ioj::radar_source::glyph(entity_type)};
            auto const contact_size_scale{ioj::radar_source::size_scale(entity_type)};
            auto const is_fighter{entity_type == ::ioj::sim::EntityType::Fighter};
            for (int32 index{}; index < batch_count; ++index, ++output_index) {
                if (::ioj::sim::is_dead(batch.health(index))) {
                    continue;
                }

                auto const id{batch.ids[index]};
                if (id == player_id) {
                    continue;
                }
                if (priority == 0) {
                    ++candidate_count;
                }

                auto const contact_flags{ioj::radar_source::flags(objective_roles[output_index])};
                if (ioj::radar_source::draw_priority(contact_flags) != priority) {
                    continue;
                }

                auto const world_delta{ioj::to_unreal(::ioj::vector_at(batch.locations, index)) -
                                       player_origin};
                if (world_delta.SizeSquared() >= range_squared) {
                    continue;
                }
                auto const local_delta{FVector3f{
                    no_roll_transform.InverseTransformVectorNoScale(FVector{world_delta})}};
                auto heading_radians{0.0f};
                if (is_fighter) {
                    auto const world_velocity{ioj::to_unreal(batch.velocity(index))};
                    auto const local_velocity{FVector3f{
                        no_roll_transform.InverseTransformVectorNoScale(FVector{world_velocity})}};
                    if (FVector2f{local_velocity.X, local_velocity.Y}.SizeSquared() >
                        UE_SMALL_NUMBER) {
                        heading_radians = FMath::Atan2(local_velocity.Y, local_velocity.X);
                    }
                }
                output_frame.instances.Add({
                    .radar_position = ioj::to_unreal(ioj::presentation::radar::to_radar_position(
                        ioj::Vector3f{local_delta}, settings, curve)),
                    .size_scale = contact_size_scale,
                    .packed_color = pack_radar_color(ioj::radar_source::colour(
                        ioj::to_unreal(batch.team(index)), player_team, contact_colours)),
                    .packed_glyph_and_flags =
                        pack_radar_display(contact_glyph, contact_flags, heading_radians),
                });
            }
        }
    }

    output_frame.instances.Add({
        .radar_position = FVector3f::ZeroVector,
        .size_scale = 1.0f,
        .packed_color = pack_radar_color(FLinearColor{1.0f, 0.72f, 0.18f, 1.0f}),
        .packed_glyph_and_flags = pack_radar_display(ERadarGlyph::Player, ERadarContactFlags::None),
    });
    return {.candidate_count = candidate_count, .visible_count = output_frame.instances.Num()};
}
