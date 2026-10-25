#pragma once

#include "ioj/presentation/agent_display_batch.h"
#include "ioj/sim/entities/entity_id.h"
#include "ioj/sim/entities/entity_type_radii.h"
#include "SpaceGamePresentation/entities/TeamColours.h"
#include <ioj/presentation/soft_target.h>

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

struct SPACEGAMEPRESENTATION_API FEntityOverlayHealthMaximums {
    int32 capital_ship{1};
    int32 fighter{1};
    int32 turret{1};
};

struct SPACEGAMEPRESENTATION_API FEntityOverlayTeamColours {
    FTeamColours capital_ship;
    FTeamColours fighter;
    FTeamColours turret;
};

struct SPACEGAMEPRESENTATION_API FEntityOverlayCollectionResult {
    int32 candidate_count{0};
    int32 invalid_health_count{0};
};

[[nodiscard]] SPACEGAMEPRESENTATION_API auto
    select_soft_target(std::span<::ioj::presentation::AgentDisplayBatch const> batches,
                       ::ioj::sim::EntityTypeRadii const& entity_type_radii,
                       TConstArrayView<EEntityOverlayObjectiveRole> objective_roles,
                       FSoftTargetSelectionContext const& context,
                       ioj::settings::SoftTarget const& settings,
                       ::ioj::sim::EntityId current_target) -> ioj::presentation::SoftTargetResult;

[[nodiscard]] SPACEGAMEPRESENTATION_API auto collect_entity_overlay_instances(
    std::span<::ioj::presentation::AgentDisplayBatch const> batches,
    ::ioj::sim::EntityTypeRadii const& entity_type_radii,
    TConstArrayView<EEntityOverlayObjectiveRole> objective_roles,
    FEntityOverlayTeamColours const& team_colours,
    FEntityOverlayHealthMaximums const& maximum_health,
    FVector3f origin,
    float maximum_range,
    TArray<FEntityOverlayInstance>& output_instances,
    FEntityOverlayCollector& collector) -> FEntityOverlayCollectionResult;
