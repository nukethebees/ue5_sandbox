#include "SpaceGame/presentation/RadarSource.h"

#include "SpaceGame/entities/TestEntityType.h"

namespace ml::radar_source {
auto glyph(ETestEntityType const type) -> ERadarGlyph {
    switch (type) {
        case ETestEntityType::PlayerShip: {
            return ERadarGlyph::Player;
        }
        case ETestEntityType::CapitalShipFighter: {
            return ERadarGlyph::Fighter;
        }
        case ETestEntityType::CapitalShip: {
            return ERadarGlyph::CapitalShip;
        }
        case ETestEntityType::Turret: {
            return ERadarGlyph::Turret;
        }
        default: {
            return ERadarGlyph::Unknown;
        }
    }
}

auto colour(ETestEntityType const type, ETestTeam const team, FRadarTeamColours const& colours)
    -> FLinearColor {
    switch (type) {
        case ETestEntityType::CapitalShip: {
            return colours.capital_ship[team];
        }
        case ETestEntityType::CapitalShipFighter: {
            return colours.fighter[team];
        }
        case ETestEntityType::Turret: {
            return colours.turret[team];
        }
        default: {
            return FLinearColor::White;
        }
    }
}

auto size_scale(ETestEntityType const type) -> float {
    switch (type) {
        case ETestEntityType::CapitalShip: {
            return 1.35f;
        }
        case ETestEntityType::Turret: {
            return 1.05f;
        }
        case ETestEntityType::CapitalShipFighter: {
            return 0.9f;
        }
        default: {
            return 0.8f;
        }
    }
}

auto flags(EEntityOverlayObjectiveRole const objective_role, bool const selected)
    -> ERadarContactFlags {
    auto result{selected ? ERadarContactFlags::Selected : ERadarContactFlags::None};
    if (objective_role == EEntityOverlayObjectiveRole::Defend) {
        result |= ERadarContactFlags::DefendObjective;
    } else if (objective_role == EEntityOverlayObjectiveRole::Destroy) {
        result |= ERadarContactFlags::DestroyObjective;
    }
    return result;
}

auto to_radar_position(FVector3f const local_delta, float const maximum_range) -> FVector3f {
    FVector2f const planar{local_delta.X, local_delta.Y};
    auto const planar_distance{planar.Size()};
    FVector2f mapped_planar{FVector2f::ZeroVector};
    if (planar_distance > UE_SMALL_NUMBER) {
        auto const direction{planar / planar_distance};
        auto const hex_denominator{
            FMath::Max(FMath::Abs(direction.X) * 0.8660254f + FMath::Abs(direction.Y) * 0.5f,
                       FMath::Abs(direction.Y))};
        auto const boundary_radius{0.8660254f / FMath::Max(hex_denominator, UE_SMALL_NUMBER)};
        mapped_planar = direction * (planar_distance / maximum_range) * boundary_radius;
    }
    return {mapped_planar.X, mapped_planar.Y, local_delta.Z / maximum_range};
}
} // namespace ml::radar_source

auto collect_radar_instances(ml::entity_registry::EntityData::ConstView const entities,
                             TConstArrayView<int32> const generations,
                             TConstArrayView<EEntityOverlayObjectiveRole> const objective_roles,
                             FRadarTeamColours const& team_colours,
                             FTransform const& player_transform,
                             FRegistryEntityHandle const player_handle,
                             FRegistryEntityHandle const selected_handle,
                             float const maximum_range,
                             TArray<FRadarInstance>& output_instances) -> FRadarCollectionResult {
    TRACE_CPUPROFILER_EVENT_SCOPE(Radar::CollectRegistrySource);
    entities.validate_array_sizes();
    check(generations.Num() == entities.num());
    check(objective_roles.Num() == entities.num());
    output_instances.Reset();

    auto const range{FMath::Max(maximum_range, UE_SMALL_NUMBER)};
    auto const range_squared{range * range};
    auto const rotation{player_transform.Rotator()};
    FTransform const no_roll_transform{FRotator{rotation.Pitch, rotation.Yaw, 0.0f},
                                       player_transform.GetLocation()};
    auto const player_origin{FVector3f{player_transform.GetLocation()}};
    auto const count{entities.num()};
    output_instances.Reserve(count + 1);

    int32 candidate_count{0};
    for (int32 priority{0}; priority < 2; ++priority) {
        for (int32 index{0}; index < count; ++index) {
            if (entities.alive[index] == 0) {
                continue;
            }

            FRegistryEntityHandle const handle{index, generations[index]};
            if (handle == player_handle ||
                entities.entity_types[index] == ETestEntityType::PlayerShip) {
                continue;
            }
            if (priority == 0) {
                ++candidate_count;
            }

            auto const contact_flags{
                ml::radar_source::flags(objective_roles[index], handle == selected_handle)};
            auto const emphasized{contact_flags != ERadarContactFlags::None};
            if (emphasized != (priority == 1)) {
                continue;
            }

            auto const world_delta{entities.locations[index] - player_origin};
            if (maximum_range <= 0.0f || world_delta.SizeSquared() >= range_squared) {
                continue;
            }
            auto const local_delta{
                FVector3f{no_roll_transform.InverseTransformVectorNoScale(FVector{world_delta})}};
            auto const entity_type{entities.entity_types[index]};
            output_instances.Add({
                .radar_position = ml::radar_source::to_radar_position(local_delta, range),
                .size_scale = ml::radar_source::size_scale(entity_type),
                .packed_color = pack_radar_color(
                    ml::radar_source::colour(entity_type, entities.teams[index], team_colours)),
                .packed_glyph_and_flags =
                    pack_radar_display(ml::radar_source::glyph(entity_type), contact_flags),
            });
        }
    }

    output_instances.Add({
        .radar_position = FVector3f::ZeroVector,
        .size_scale = 1.0f,
        .packed_color = pack_radar_color(FLinearColor{1.0f, 0.72f, 0.18f, 1.0f}),
        .packed_glyph_and_flags = pack_radar_display(ERadarGlyph::Player, ERadarContactFlags::None),
    });
    return {.candidate_count = candidate_count, .visible_count = output_instances.Num()};
}
