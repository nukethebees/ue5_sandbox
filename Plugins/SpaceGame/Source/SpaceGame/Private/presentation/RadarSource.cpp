#include "SpaceGame/presentation/RadarSource.h"

#include "SpaceGame/entities/TestEntityType.h"

namespace ml::radar_source {
inline constexpr float hex_inradius{0.8660254f};

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

auto curve_parameter(FRadarSettings const& settings) -> float {
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

auto to_radar_display_radius(float const world_distance, FRadarSettings const& settings) -> float {
    return map_distance(world_distance / settings.maximum_range, curve_parameter(settings));
}

auto to_radar_position(FVector3f const local_delta,
                       FRadarSettings const& settings,
                       float const curve) -> FVector3f {
    FVector2f const planar{local_delta.X, local_delta.Y};
    auto const planar_distance{planar.Size()};
    FVector2f mapped_planar{FVector2f::ZeroVector};
    if (planar_distance > UE_SMALL_NUMBER) {
        auto const direction{planar / planar_distance};
        auto const hex_denominator{
            FMath::Max(FMath::Abs(direction.X) * hex_inradius + FMath::Abs(direction.Y) * 0.5f,
                       FMath::Abs(direction.Y))};
        auto const boundary_radius{hex_inradius / FMath::Max(hex_denominator, UE_SMALL_NUMBER)};
        mapped_planar = direction * map_distance(planar_distance / settings.maximum_range, curve) *
                        boundary_radius;
    }
    auto const normalized_height{FMath::Abs(local_delta.Z) / settings.maximum_range};
    auto const mapped_height{map_distance(normalized_height, curve)};
    return {
        mapped_planar.X, mapped_planar.Y, local_delta.Z < 0.0f ? -mapped_height : mapped_height};
}

auto to_radar_position(FVector3f const local_delta, FRadarSettings const& settings) -> FVector3f {
    return to_radar_position(local_delta, settings, curve_parameter(settings));
}
} // namespace ml::radar_source

auto sanitize_radar_settings(FRadarSettings settings) -> FRadarSettings {
    settings.maximum_range = FMath::Max(settings.maximum_range, UE_SMALL_NUMBER);
    settings.combat_range =
        FMath::Clamp(settings.combat_range, UE_SMALL_NUMBER, settings.maximum_range);
    settings.tactical_range =
        FMath::Clamp(settings.tactical_range, settings.combat_range, settings.maximum_range);
    auto const linear_combat_radius{settings.combat_range / settings.maximum_range};
    settings.combat_display_radius = linear_combat_radius >= 1.0f - UE_KINDA_SMALL_NUMBER
                                       ? linear_combat_radius
                                       : FMath::Clamp(settings.combat_display_radius,
                                                      linear_combat_radius,
                                                      1.0f - UE_KINDA_SMALL_NUMBER);
    settings.grid_opacity = FMath::Clamp(settings.grid_opacity, 0.0f, 1.0f);
    settings.strategic_cell_radius = FMath::Clamp(settings.strategic_cell_radius, 0.01f, 1.0f);
    settings.tactical_cell_radius =
        FMath::Clamp(settings.tactical_cell_radius, settings.strategic_cell_radius, 1.0f);
    settings.combat_cell_radius =
        FMath::Clamp(settings.combat_cell_radius, settings.tactical_cell_radius, 1.0f);
    settings.core_cell_radius =
        FMath::Clamp(settings.core_cell_radius, settings.combat_cell_radius, 1.0f);
    return settings;
}

auto collect_radar_instances(ml::entity_registry::EntityData::ConstView const entities,
                             TConstArrayView<int32> const generations,
                             TConstArrayView<EEntityOverlayObjectiveRole> const objective_roles,
                             FRadarContactColours const& contact_colours,
                             FTransform const& player_transform,
                             FRegistryEntityHandle const player_handle,
                             FRegistryEntityHandle const selected_handle,
                             ETestTeam const player_team,
                             FRadarSettings const& settings,
                             FRadarFrame& output_frame) -> FRadarCollectionResult {
    TRACE_CPUPROFILER_EVENT_SCOPE(Radar::CollectRegistrySource);
    entities.validate_array_sizes();
    check(generations.Num() == entities.num());
    check(objective_roles.Num() == entities.num());
    output_frame.instances.Reset();

    auto const rotation{player_transform.Rotator()};
    FTransform const no_roll_transform{FRotator{rotation.Pitch, rotation.Yaw, 0.0f},
                                       player_transform.GetLocation()};
    auto const player_origin{FVector3f{player_transform.GetLocation()}};
    auto const curve{ml::radar_source::curve_parameter(settings)};
    output_frame.combat_display_radius = settings.combat_display_radius;
    output_frame.tactical_display_radius =
        ml::radar_source::map_distance(settings.tactical_range / settings.maximum_range, curve);
    auto const range_squared{settings.maximum_range * settings.maximum_range};
    auto const count{entities.num()};
    output_frame.instances.Reserve(count + 1);

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
            if (world_delta.SizeSquared() >= range_squared) {
                continue;
            }
            auto const local_delta{
                FVector3f{no_roll_transform.InverseTransformVectorNoScale(FVector{world_delta})}};
            auto const entity_type{entities.entity_types[index]};
            output_frame.instances.Add({
                .radar_position = ml::radar_source::to_radar_position(local_delta, settings, curve),
                .size_scale = ml::radar_source::size_scale(entity_type),
                .packed_color = pack_radar_color(
                    ml::radar_source::colour(entities.teams[index], player_team, contact_colours)),
                .packed_glyph_and_flags =
                    pack_radar_display(ml::radar_source::glyph(entity_type), contact_flags),
            });
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
