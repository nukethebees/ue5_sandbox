#include "SpaceGame/presentation/EntityOverlaySource.h"

#include "SpaceGame/entities/TestEntityType.h"

#include <limits>
#include <utility>

namespace {
struct FProjectedPosition {
    FVector2f pixels{FVector2f::ZeroVector};
    bool on_screen{false};
};

struct FSoftTargetCandidate {
    FRegistryEntityHandle handle{};
    float centre_distance_pixels{std::numeric_limits<float>::max()};
    float centre_score_pixels{std::numeric_limits<float>::max()};
    float surface_distance{std::numeric_limits<float>::max()};
    float range_progress{0.0f};
    float indicator_radius_pixels{0.0f};
    bool in_range{false};
};

auto is_better_candidate(FSoftTargetCandidate const& candidate,
                         FSoftTargetCandidate const& incumbent) -> bool {
    if (candidate.centre_score_pixels != incumbent.centre_score_pixels) {
        return candidate.centre_score_pixels < incumbent.centre_score_pixels;
    }
    if (candidate.surface_distance != incumbent.surface_distance) {
        return candidate.surface_distance < incumbent.surface_distance;
    }
    return candidate.handle.index < incumbent.handle.index;
}

auto project_to_overlay(FEntityOverlayView const& view,
                        FVector3f const world_position,
                        FProjectedPosition& result) -> bool {
    auto const relative{world_position - view.camera_origin};
    auto const& matrix{view.view_projection.M};
    FVector4f const projected{relative.X * matrix[0][0] + relative.Y * matrix[1][0] +
                                  relative.Z * matrix[2][0] + matrix[3][0],
                              relative.X * matrix[0][1] + relative.Y * matrix[1][1] +
                                  relative.Z * matrix[2][1] + matrix[3][1],
                              relative.X * matrix[0][2] + relative.Y * matrix[1][2] +
                                  relative.Z * matrix[2][2] + matrix[3][2],
                              relative.X * matrix[0][3] + relative.Y * matrix[1][3] +
                                  relative.Z * matrix[2][3] + matrix[3][3]};
    if (!FMath::IsFinite(projected.X) || !FMath::IsFinite(projected.Y) ||
        !FMath::IsFinite(projected.W) || projected.W <= UE_SMALL_NUMBER) {
        return false;
    }

    auto const inverse_w{1.0f / projected.W};
    FVector2f const ndc{projected.X * inverse_w, projected.Y * inverse_w};
    auto const view_min{FVector2f{view.view_rect.Min}};
    auto const view_size{FVector2f{view.view_rect.Size()}};
    result.pixels = view_min + FVector2f{ndc.X * 0.5f + 0.5f, 0.5f - ndc.Y * 0.5f} * view_size;
    result.on_screen = ndc.X >= -1.0f && ndc.X <= 1.0f && ndc.Y >= -1.0f && ndc.Y <= 1.0f;
    return true;
}

auto is_supported_entity_type(ETestEntityType const type) -> bool {
    return type == ETestEntityType::CapitalShip || type == ETestEntityType::CapitalShipFighter ||
           type == ETestEntityType::Turret;
}

auto inverse_maximum_health(ETestEntityType const type,
                            FEntityOverlayHealthMaximums const& maximums) -> float {
    int32 maximum{0};
    switch (type) {
        case ETestEntityType::CapitalShip: {
            maximum = maximums.capital_ship;
            break;
        }
        case ETestEntityType::CapitalShipFighter: {
            maximum = maximums.fighter;
            break;
        }
        case ETestEntityType::Turret: {
            maximum = maximums.turret;
            break;
        }
        default: {
            return 0.0f;
        }
    }
    return maximum > 0 ? 1.0f / static_cast<float>(maximum) : 0.0f;
}

auto team_colour(ETestEntityType const type,
                 ETestTeam const team,
                 FEntityOverlayTeamColours const& colours) -> FLinearColor {
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
}

auto select_soft_target(ml::entity_registry::EntityData::ConstView const entities,
                        TConstArrayView<int> const generations,
                        TConstArrayView<EEntityOverlayObjectiveRole> const objective_roles,
                        FSoftTargetSelectionContext const& context,
                        FSoftTargetSelectionSettings const& settings,
                        FRegistryEntityHandle const current_target) -> FSoftTargetSelectionResult {
    entities.validate_array_sizes();
    check(generations.Num() == entities.num());
    check(objective_roles.Num() == entities.num());
    if (!context.view.is_valid()) {
        return {};
    }

    auto const aim_direction{context.aim_direction.GetSafeNormal()};
    if (aim_direction.IsNearlyZero()) {
        return {};
    }

    auto const acquisition_radius{FMath::Max(settings.acquisition_radius_pixels, 1.0f)};
    auto const retention_radius{FMath::Max(settings.retention_radius_pixels, acquisition_radius)};
    auto const centre_tie_radius{FMath::Max(settings.centre_tie_radius_pixels, 0.0f)};
    auto const switch_ratio{FMath::Clamp(settings.switch_improvement_ratio, 0.0f, 1.0f)};
    auto const minimum_indicator_radius{FMath::Max(settings.minimum_indicator_radius_pixels, 1.0f)};
    auto const maximum_indicator_radius{
        FMath::Max(settings.maximum_indicator_radius_pixels, minimum_indicator_radius)};
    auto const bounds_padding{FMath::Max(settings.bounds_padding_pixels, 0.0f)};
    auto const maximum_overlay_range{FMath::Max(context.maximum_overlay_range, 0.0f)};
    auto const maximum_overlay_range_squared{maximum_overlay_range * maximum_overlay_range};
    auto const effective_range{FMath::Max(context.effective_weapon_range, 0.0f)};
    auto const approach_multiplier{FMath::Max(settings.approach_range_multiplier, 1.0f)};

    FSoftTargetCandidate best_candidate;
    FSoftTargetCandidate current_candidate;
    bool has_best_candidate{false};
    bool has_current_candidate{false};

    auto const count{entities.num()};
    for (int32 index{0}; index < count; ++index) {
        if (entities.alive[index] == 0 || entities.teams[index] == context.player_team ||
            !is_supported_entity_type(entities.entity_types[index])) {
            continue;
        }

        auto const position{entities.locations[index]};
        auto const objective{objective_roles[index] != EEntityOverlayObjectiveRole::None};
        if (!objective && FVector3f::DistSquared(context.view.camera_origin, position) >
                              maximum_overlay_range_squared) {
            continue;
        }

        auto const distance_along_aim{
            FVector3f::DotProduct(position - context.aim_origin, aim_direction)};
        if (distance_along_aim <= 0.0f) {
            continue;
        }

        FProjectedPosition projected_entity;
        if (!project_to_overlay(context.view, position, projected_entity) ||
            !projected_entity.on_screen) {
            continue;
        }

        FProjectedPosition projected_aim;
        auto const aim_position{context.aim_origin + aim_direction * distance_along_aim};
        if (!project_to_overlay(context.view, aim_position, projected_aim)) {
            continue;
        }

        auto const world_radius{FMath::Max(entities.radii[index], 0.0f)};
        float projected_radius{};
        FProjectedPosition projected_edge;
        if (project_to_overlay(
                context.view, position + context.camera_right * world_radius, projected_edge)) {
            projected_radius = FVector2f::Distance(projected_entity.pixels, projected_edge.pixels);
        }
        if (project_to_overlay(
                context.view, position + context.camera_up * world_radius, projected_edge)) {
            projected_radius =
                FMath::Max(projected_radius,
                           FVector2f::Distance(projected_entity.pixels, projected_edge.pixels));
        }
        if (!FMath::IsFinite(projected_radius)) {
            projected_radius = 0.0f;
        }

        auto const centre_distance{
            FVector2f::Distance(projected_entity.pixels, projected_aim.pixels)};
        auto const centre_score{FMath::Max(centre_distance - centre_tie_radius, 0.0f)};
        auto const surface_distance{
            FMath::Max(FVector3f::Distance(context.aim_origin, position) - world_radius, 0.0f)};
        auto const in_range{effective_range > 0.0f && surface_distance <= effective_range};

        float range_progress{};
        if (in_range) {
            range_progress = 1.0f;
        } else if (effective_range > 0.0f && approach_multiplier > 1.0f) {
            auto const approach_distance{effective_range * approach_multiplier};
            auto const linear_progress{FMath::Clamp((approach_distance - surface_distance) /
                                                        (approach_distance - effective_range),
                                                    0.0f,
                                                    1.0f)};
            range_progress = linear_progress;
        }

        FSoftTargetCandidate const candidate{.handle = {index, generations[index]},
                                             .centre_distance_pixels = centre_distance,
                                             .centre_score_pixels = centre_score,
                                             .surface_distance = surface_distance,
                                             .range_progress = range_progress,
                                             .indicator_radius_pixels =
                                                 FMath::Clamp(projected_radius + bounds_padding,
                                                              minimum_indicator_radius,
                                                              maximum_indicator_radius),
                                             .in_range = in_range};

        if (candidate.handle == current_target && centre_distance <= retention_radius) {
            current_candidate = candidate;
            has_current_candidate = true;
        }
        if (centre_distance <= acquisition_radius &&
            (!has_best_candidate || is_better_candidate(candidate, best_candidate))) {
            best_candidate = candidate;
            has_best_candidate = true;
        }
    }

    FSoftTargetCandidate selected_candidate;
    bool has_selected_candidate{false};
    if (has_current_candidate) {
        selected_candidate = current_candidate;
        has_selected_candidate = true;
        if (has_best_candidate && best_candidate.handle != current_candidate.handle) {
            auto const screen_improvement{best_candidate.centre_score_pixels <
                                          current_candidate.centre_score_pixels * switch_ratio};
            auto const tied_centres{best_candidate.centre_score_pixels ==
                                    current_candidate.centre_score_pixels};
            auto const depth_improvement{best_candidate.surface_distance <
                                         current_candidate.surface_distance * switch_ratio};
            if (screen_improvement || (tied_centres && depth_improvement)) {
                selected_candidate = best_candidate;
            }
        }
    } else if (has_best_candidate) {
        selected_candidate = best_candidate;
        has_selected_candidate = true;
    }

    if (!has_selected_candidate) {
        return {};
    }
    return {.handle = selected_candidate.handle,
            .range_progress = selected_candidate.range_progress,
            .indicator_radius_pixels = selected_candidate.indicator_radius_pixels,
            .in_range = selected_candidate.in_range};
}

auto collect_entity_overlay_instances(
    ml::entity_registry::EntityData::ConstView const entities,
    TConstArrayView<EEntityOverlayObjectiveRole> const objective_roles,
    FEntityOverlayTeamColours const& team_colours,
    FEntityOverlayHealthMaximums const& maximum_health,
    FVector3f const origin,
    float const maximum_range,
    TArray<FEntityOverlayInstance>& output_instances,
    FEntityOverlayCollector& collector,
    int32 const soft_target_entity_index) -> FEntityOverlayCollectionResult {
    TRACE_CPUPROFILER_EVENT_SCOPE(EntityOverlay::CollectRegistrySource);
    entities.validate_array_sizes();
    check(objective_roles.Num() == entities.num());
    collector.begin(origin, FMath::Max(maximum_range, 0.0f), output_instances);

    auto const count{entities.num()};
    output_instances.Reserve(count);
    for (int32 index{0}; index < count; ++index) {
        if (entities.alive[index] == 0) {
            continue;
        }

        auto const inverse_health{
            inverse_maximum_health(entities.entity_types[index], maximum_health)};
        if (inverse_health <= 0.0f) {
            continue;
        }

        auto const objective_role{objective_roles[index]};
        static_cast<void>(collector.try_add_colored(
            entities.locations[index],
            static_cast<float>(entities.healths[index]) * inverse_health,
            entities.radii[index],
            team_colour(entities.entity_types[index], entities.teams[index], team_colours),
            objective_role,
            objective_role != EEntityOverlayObjectiveRole::None,
            index == soft_target_entity_index));
    }

    return {.candidate_count = output_instances.Num(),
            .invalid_health_count = collector.invalid_health_count()};
}
