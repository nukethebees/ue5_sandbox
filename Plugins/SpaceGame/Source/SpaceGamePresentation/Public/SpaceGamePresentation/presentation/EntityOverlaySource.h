#pragma once

#include "ioj/presentation/agent_display_batch.h"
#include "ioj/sim/entities/entity_id.h"
#include "ioj/sim/entities/entity_type_radii.h"
#include <ioj/presentation/soft_target.h>
#include <SpaceGamePresentation/entities/TestTeam.h>

#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

#include <span>

struct SPACEGAMEPRESENTATION_API FSoftTargetSelectionContext {
    FEntityOverlayView view;
    FVector3f aim_origin{FVector3f::ZeroVector};
    FVector3f aim_direction{1.0f, 0.0f, 0.0f};
    FVector3f camera_right{0.0f, 1.0f, 0.0f};
    FVector3f camera_up{0.0f, 0.0f, 1.0f};
    ETestTeam player_team{ETestTeam::White};
    float effective_weapon_range{0.0f};
    float maximum_overlay_range{0.0f};
};

[[nodiscard]] SPACEGAMEPRESENTATION_API auto
    select_soft_target(std::span<::ioj::presentation::AgentDisplayBatch const> batches,
                       ::ioj::sim::EntityTypeRadii const& entity_type_radii,
                       std::span<ioj::ui::entity_overlay::ObjectiveRole const> objective_roles,
                       FSoftTargetSelectionContext const& context,
                       ioj::presentation::SoftTargetConfig const& settings,
                       ::ioj::sim::EntityId current_target) -> ioj::presentation::SoftTargetResult;
