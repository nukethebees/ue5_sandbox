#include "SpaceGamePresentation/presentation/EntityOverlaySource.h"

#include <SpaceGamePresentation/entities/TestTeamConversion.h>

#include <SandboxCoreEngine/VectorConversion.h>

#include <algorithm>

auto select_soft_target(
    std::span<ioj::presentation::AgentDisplayBatch const> const batches,
    ioj::sim::EntityTypeRadii const& entity_type_radii,
    std::span<ioj::ui::entity_overlay::ObjectiveRole const> const objective_roles,
    FSoftTargetSelectionContext const& context,
    ioj::presentation::SoftTargetConfig const& settings,
    ioj::sim::EntityId const current_target) -> ioj::presentation::SoftTargetResult {
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
        batches, entity_type_radii, objective_roles, native, settings, current_target);
}
