#include <ioj/presentation/soft_target.h>

#include <ioj/sim/entities/health.h>
#include <ioj/vector_value_math.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>

namespace ioj::presentation {
namespace {
struct ProjectedPosition {
    Vector2f pixels{};
    bool on_screen{false};
};

struct SoftTargetCandidate {
    static constexpr float no_candidate_distance{std::numeric_limits<float>::infinity()};

    ::ioj::sim::EntityId id{};
    float centre_distance_pixels{no_candidate_distance};
    float centre_score_pixels{no_candidate_distance};
    float surface_distance{no_candidate_distance};
    float range_alpha{0.0f};
    float indicator_radius_pixels{0.0f};
    float world_units_per_pixel{0.0f};
};

auto is_better_candidate(SoftTargetCandidate const& candidate, SoftTargetCandidate const& incumbent)
    -> bool {
    if (candidate.centre_score_pixels != incumbent.centre_score_pixels) {
        return candidate.centre_score_pixels < incumbent.centre_score_pixels;
    }
    if (candidate.surface_distance != incumbent.surface_distance) {
        return candidate.surface_distance < incumbent.surface_distance;
    }
    return candidate.id < incumbent.id;
}

auto project_to_overlay(OverlayView const& view,
                        Vector3f const world_position,
                        ProjectedPosition& result) -> bool {
    auto const relative{world_position - view.camera_origin};
    auto const& matrix{view.view_projection};
    Vector<float, 4> const projected{relative.x() * matrix[0][0] + relative.y() * matrix[1][0] +
                                         relative.z() * matrix[2][0] + matrix[3][0],
                                     relative.x() * matrix[0][1] + relative.y() * matrix[1][1] +
                                         relative.z() * matrix[2][1] + matrix[3][1],
                                     relative.x() * matrix[0][2] + relative.y() * matrix[1][2] +
                                         relative.z() * matrix[2][2] + matrix[3][2],
                                     relative.x() * matrix[0][3] + relative.y() * matrix[1][3] +
                                         relative.z() * matrix[2][3] + matrix[3][3]};
    if (!std::isfinite(projected.x()) || !std::isfinite(projected.y()) ||
        !std::isfinite(projected.w()) || projected.w() <= 1.e-8f) {
        return false;
    }

    auto const inverse_w{1.0f / projected.w()};
    Vector2f const ndc{projected.x() * inverse_w, projected.y() * inverse_w};
    auto const view_min{view.minimum};
    auto const view_size{view.size};
    result.pixels = view_min + Vector2f{ndc.x() * 0.5f + 0.5f, 0.5f - ndc.y() * 0.5f} * view_size;
    result.on_screen = ndc.x() >= -1.0f && ndc.x() <= 1.0f && ndc.y() >= -1.0f && ndc.y() <= 1.0f;
    return true;
}

auto is_supported_entity_type(::ioj::sim::EntityType const type) -> bool {
    return type == ::ioj::sim::EntityType::CapitalShip || type == ::ioj::sim::EntityType::Fighter ||
           type == ::ioj::sim::EntityType::Turret;
}

}

auto SoftTargetState::advance(std::span<AgentDisplayBatch const> const batches,
                              SoftTargetResult const& selected,
                              float const delta_seconds,
                              float const pulse_duration,
                              float const fade_duration) -> std::array<SoftTargetMarker, 2> {
    sim::EntityId const ids[]{active.entity, fading.entity, selected.id};
    std::optional<Vector3f> positions[3];
    for (auto const& batch : batches) {
        auto const locations{batch.locations};
        auto const count{batch.num()};
        for (std::uint32_t row{}; row < count; ++row) {
            if (!sim::is_alive(batch.health(row))) {
                continue;
            }
            for (std::uint32_t target{}; target < 3; ++target) {
                if (batch.ids[row] == ids[target]) {
                    positions[target] = locations[row];
                }
            }
        }
    }
    auto active_location{positions[0]};
    auto fading_location{positions[1]};
    auto const nonnegative = [](float const value) { return value > 0.f ? value : 0.f; };
    auto const elapsed{nonnegative(delta_seconds)};
    auto const clear_fading = [this] {
        fading = {};
        fade_remaining = 0.f;
    };
    pulse_remaining = nonnegative(pulse_remaining - elapsed);
    if (fading.entity.is_valid()) {
        fade_remaining = nonnegative(fade_remaining - elapsed);
        if (!fading_location || fade_remaining <= 0.f) {
            clear_fading();
        }
    }

    auto const begin_fade = [&] {
        if (!active.entity.is_valid() || !active_location || fade_duration <= 0.f) {
            return;
        }
        fading = active;
        fading_location = active_location;
        fade_remaining = fade_duration;
    };
    if (selected.id.is_valid()) {
        if (selected.id != active.entity) {
            if (selected.id == fading.entity) {
                clear_fading();
            }
            begin_fade();
            pulse_remaining = 0.f;
        } else if (active.range_alpha < 1.f && selected.range_alpha >= 1.f) {
            pulse_remaining = pulse_duration;
        }
        active = {.entity = selected.id,
                  .range_alpha =
                      nonnegative(selected.range_alpha < 1.f ? selected.range_alpha : 1.f),
                  .radius_pixels = selected.indicator_radius_pixels,
                  .world_units_per_pixel = selected.world_units_per_pixel};
        active_location = positions[2];
    } else {
        if (selected.previous_target_can_fade) {
            begin_fade();
        }
        active = {};
        pulse_remaining = 0.f;
    }

    return {
        SoftTargetMarker{.visual = active,
                         .location = active.entity.is_valid() ? active_location : std::nullopt,
                         .pulse = pulse_duration > 0.f ? pulse_remaining / pulse_duration : 0.f,
                         .visibility = 1.f},
        SoftTargetMarker{.visual = fading,
                         .location = fading.entity.is_valid() ? fading_location : std::nullopt,
                         .visibility = fade_duration > 0.f ? fade_remaining / fade_duration : 0.f}};
}

auto select_soft_target(std::span<::ioj::presentation::AgentDisplayBatch const> const batches,
                        ::ioj::sim::EntityTypeRadii const& entity_type_radii,
                        std::span<ui::entity_overlay::ObjectiveRole const> const objective_roles,
                        SoftTargetContext const& context,
                        settings::SoftTarget const& settings,
                        ::ioj::sim::EntityId const current_target) -> SoftTargetResult {
    assert(objective_roles.size() == ::ioj::presentation::display_entity_count(batches));
    if (!context.view.is_valid()) {
        return {};
    }

    auto const aim_direction{safe_normal(context.aim_direction)};
    if (size_squared(aim_direction) <= 1.e-8f) {
        return {};
    }

    auto const acquisition_radius{std::max(settings.acquisition_radius_pixels, 1.0f)};
    auto const retention_radius{std::max(settings.retention_radius_pixels, acquisition_radius)};
    auto const centre_tie_radius{std::max(settings.centre_tie_radius_pixels, 0.0f)};
    auto const switch_ratio{std::clamp(settings.switch_improvement_ratio, 0.0f, 1.0f)};
    auto const minimum_indicator_radius{std::max(settings.minimum_radius_pixels, 1.0f)};
    auto const maximum_indicator_radius{
        std::max(settings.maximum_radius_pixels, minimum_indicator_radius)};
    auto const bounds_padding{std::max(settings.bounds_padding_pixels, 0.0f)};
    auto const maximum_overlay_range{std::max(context.maximum_overlay_range, 0.0f)};
    auto const maximum_overlay_range_squared{maximum_overlay_range * maximum_overlay_range};
    auto const effective_range{std::max(context.effective_weapon_range, 0.0f)};
    auto const transition_start_multiplier{
        std::max(settings.range_transition_start_multiplier, 1.0f)};

    SoftTargetCandidate best_candidate;
    SoftTargetCandidate current_candidate;
    bool has_best_candidate{false};
    bool has_current_candidate{false};

    std::uint32_t output_index{};
    for (auto const& batch : batches) {
        auto const batch_count{static_cast<std::uint32_t>(batch.num())};
        assert(batch.ids.size() == static_cast<std::size_t>(batch_count));
        if (!is_supported_entity_type(batch.type)) {
            output_index += batch_count;
            continue;
        }

        auto const world_radius{std::max(entity_type_radii[batch.type], 0.0f)};
        auto const right_offset{context.camera_right * world_radius};
        auto const up_offset{context.camera_up * world_radius};
        for (std::uint32_t index{}; index < batch_count; ++index, ++output_index) {
            if (::ioj::sim::is_dead(batch.health(index)) ||
                batch.team(index) == context.player_team) {
                continue;
            }

            auto const position{::ioj::vector_at(batch.locations, index)};
            auto const objective{objective_roles[output_index] !=
                                 ui::entity_overlay::ObjectiveRole::None};
            if (!objective && size_squared(context.view.camera_origin - position) >
                                  maximum_overlay_range_squared) {
                continue;
            }

            auto const distance_along_aim{dot(position - context.aim_origin, aim_direction)};
            if (distance_along_aim <= 0.0f) {
                continue;
            }

            ProjectedPosition projected_entity;
            if (!project_to_overlay(context.view, position, projected_entity) ||
                !projected_entity.on_screen) {
                continue;
            }

            ProjectedPosition projected_aim;
            auto const aim_position{context.aim_origin + aim_direction * distance_along_aim};
            if (!project_to_overlay(context.view, aim_position, projected_aim)) {
                continue;
            }

            float projected_radius{};
            ProjectedPosition projected_edge;
            if (project_to_overlay(context.view, position + right_offset, projected_edge)) {
                projected_radius = size(projected_entity.pixels - projected_edge.pixels);
            }
            if (project_to_overlay(context.view, position + up_offset, projected_edge)) {
                projected_radius = std::max(projected_radius,
                                            size(projected_entity.pixels - projected_edge.pixels));
            }
            if (!std::isfinite(projected_radius)) {
                projected_radius = 0.0f;
            }

            float world_units_per_pixel{};
            if (world_radius > 1.e-8f && projected_radius > 1.e-8f) {
                world_units_per_pixel = world_radius / projected_radius;
            } else if (project_to_overlay(
                           context.view, position + context.camera_right, projected_edge)) {
                auto const projected_unit_radius{
                    size(projected_entity.pixels - projected_edge.pixels)};
                if (projected_unit_radius > 1.e-8f) {
                    world_units_per_pixel = 1.0f / projected_unit_radius;
                }
            }

            auto const centre_distance{size(projected_entity.pixels - projected_aim.pixels)};
            auto const centre_score{std::max(centre_distance - centre_tie_radius, 0.0f)};
            auto const surface_distance{
                std::max(size(context.aim_origin - position) - world_radius, 0.0f)};
            float range_alpha{};
            if (effective_range > 0.0f) {
                if (surface_distance <= effective_range) {
                    range_alpha = 1.0f;
                } else if (transition_start_multiplier > 1.0f) {
                    auto const transition_start{effective_range * transition_start_multiplier};
                    range_alpha = std::clamp((transition_start - surface_distance) /
                                                 (transition_start - effective_range),
                                             0.0f,
                                             1.0f);
                }
            }

            SoftTargetCandidate const candidate{.id = batch.ids[index],
                                                .centre_distance_pixels = centre_distance,
                                                .centre_score_pixels = centre_score,
                                                .surface_distance = surface_distance,
                                                .range_alpha = std::clamp(range_alpha, 0.0f, 1.0f),
                                                .indicator_radius_pixels =
                                                    std::clamp(projected_radius + bounds_padding,
                                                               minimum_indicator_radius,
                                                               maximum_indicator_radius),
                                                .world_units_per_pixel = world_units_per_pixel};

            if (candidate.id == current_target) {
                current_candidate = candidate;
                has_current_candidate = true;
            }
            if (centre_distance <= acquisition_radius &&
                (!has_best_candidate || is_better_candidate(candidate, best_candidate))) {
                best_candidate = candidate;
                has_best_candidate = true;
            }
        }
    }

    SoftTargetCandidate selected_candidate;
    bool has_selected_candidate{false};
    if (has_current_candidate && current_candidate.centre_distance_pixels <= retention_radius) {
        selected_candidate = current_candidate;
        has_selected_candidate = true;
        if (has_best_candidate && best_candidate.id != current_candidate.id) {
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
        if (has_current_candidate) {
            return {.range_alpha = current_candidate.range_alpha,
                    .indicator_radius_pixels = current_candidate.indicator_radius_pixels,
                    .world_units_per_pixel = current_candidate.world_units_per_pixel,
                    .previous_target_can_fade = true};
        }
        return {};
    }
    return {.id = selected_candidate.id,
            .range_alpha = selected_candidate.range_alpha,
            .indicator_radius_pixels = selected_candidate.indicator_radius_pixels,
            .world_units_per_pixel = selected_candidate.world_units_per_pixel};
}

}
