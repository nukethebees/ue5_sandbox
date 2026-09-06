#include "SpaceGame/presentation/RadarSource.h"

#include "SpaceGame/entities/TestEntityType.h"

namespace ml::radar_source {
inline constexpr float automatic_range_padding{1.15f};
inline constexpr float automatic_range_percentile{0.9f};
inline constexpr float automatic_range_step{100000.0f};

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

auto colour(ETestTeam const team, ETestTeam const player_team, FRadarContactColours const& colours)
    -> FLinearColor {
    if (team == ETestTeam::White) {
        return colours.neutral;
    }
    return team == player_team ? colours.friendly : colours.hostile;
}

auto size_scale(ETestEntityType const type) -> float {
    switch (type) {
        case ETestEntityType::CapitalShip: {
            return 1.1f;
        }
        case ETestEntityType::Turret: {
            return 1.0f;
        }
        case ETestEntityType::CapitalShipFighter: {
            return 1.0f;
        }
        default: {
            return 0.9f;
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

auto draw_priority(ERadarContactFlags const contact_flags) -> int32 {
    if (EnumHasAnyFlags(contact_flags, ERadarContactFlags::Selected)) {
        return 2;
    }
    if (EnumHasAnyFlags(contact_flags,
                        ERadarContactFlags::DefendObjective |
                            ERadarContactFlags::DestroyObjective)) {
        return 1;
    }
    return 0;
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

auto calculate_display_range(ml::entity_registry::EntityData::ConstView const entities,
                             TConstArrayView<int32> const generations,
                             FRegistryEntityHandle const player_handle,
                             FRegistryEntityHandle const selected_handle,
                             FVector3f const player_origin,
                             float const minimum_range,
                             float const maximum_range) -> float {
    TArray<float, TInlineAllocator<128>> distances;
    auto selected_distance{0.0f};
    auto const maximum_range_squared{maximum_range * maximum_range};
    auto const count{entities.num()};
    distances.Reserve(count);
    for (int32 index{0}; index < count; ++index) {
        if (entities.alive[index] == 0 ||
            entities.entity_types[index] == ETestEntityType::PlayerShip) {
            continue;
        }

        FRegistryEntityHandle const handle{index, generations[index]};
        if (handle == player_handle) {
            continue;
        }

        auto const distance_squared{(entities.locations[index] - player_origin).SizeSquared()};
        if (distance_squared >= maximum_range_squared) {
            continue;
        }

        auto const distance{FMath::Sqrt(distance_squared)};
        distances.Add(distance);
        if (handle == selected_handle) {
            selected_distance = distance;
        }
    }

    auto desired_range{0.0f};
    if (!distances.IsEmpty()) {
        distances.Sort();
        auto const percentile_index{FMath::Clamp(
            FMath::CeilToInt(static_cast<float>(distances.Num()) * automatic_range_percentile) - 1,
            0,
            distances.Num() - 1)};
        desired_range = distances[percentile_index];
    }
    desired_range = FMath::Max(
        minimum_range, FMath::Max(desired_range, selected_distance) * automatic_range_padding);
    auto const stepped_range{FMath::CeilToFloat(desired_range / automatic_range_step) *
                             automatic_range_step};
    return FMath::Clamp(stepped_range, minimum_range, maximum_range);
}
} // namespace ml::radar_source

auto collect_radar_instances(ml::entity_registry::EntityData::ConstView const entities,
                             TConstArrayView<int32> const generations,
                             TConstArrayView<EEntityOverlayObjectiveRole> const objective_roles,
                             FRadarContactColours const& contact_colours,
                             FTransform const& player_transform,
                             FRegistryEntityHandle const player_handle,
                             FRegistryEntityHandle const selected_handle,
                             ETestTeam const player_team,
                             bool const automatic_range,
                             float const minimum_range,
                             float const maximum_range,
                             TArray<FRadarInstance>& output_instances) -> FRadarCollectionResult {
    TRACE_CPUPROFILER_EVENT_SCOPE(Radar::CollectRegistrySource);
    entities.validate_array_sizes();
    check(generations.Num() == entities.num());
    check(objective_roles.Num() == entities.num());
    output_instances.Reset();

    auto const range_limit{FMath::Max(maximum_range, UE_SMALL_NUMBER)};
    auto const range_floor{FMath::Clamp(minimum_range, UE_SMALL_NUMBER, range_limit)};
    auto const rotation{player_transform.Rotator()};
    FTransform const no_roll_transform{FRotator{rotation.Pitch, rotation.Yaw, 0.0f},
                                       player_transform.GetLocation()};
    auto const player_origin{FVector3f{player_transform.GetLocation()}};
    auto const range{automatic_range && maximum_range > 0.0f
                         ? ml::radar_source::calculate_display_range(entities,
                                                                     generations,
                                                                     player_handle,
                                                                     selected_handle,
                                                                     player_origin,
                                                                     range_floor,
                                                                     range_limit)
                         : range_limit};
    auto const range_squared{range * range};
    auto const count{entities.num()};
    output_instances.Reserve(count + 1);

    int32 candidate_count{0};
    for (int32 priority{0}; priority < 3; ++priority) {
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
            if (ml::radar_source::draw_priority(contact_flags) != priority) {
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
                    ml::radar_source::colour(entities.teams[index], player_team, contact_colours)),
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
