#pragma once

#include "SpaceGamePresentation/entities/TeamColours.h"
#include "SpaceGameSimulation/entities/TestEntityRegistryData.h"

#include "SandboxNative/RegistryEntityHandle.h"
#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

struct SPACEGAMEPRESENTATION_API FSoftTargetSelectionSettings {
    float acquisition_radius_pixels{72.0f};
    float retention_radius_pixels{96.0f};
    float centre_tie_radius_pixels{4.0f};
    float switch_improvement_ratio{0.75f};
    float range_transition_start_multiplier{4.0f};
    float minimum_indicator_radius_pixels{28.0f};
    float maximum_indicator_radius_pixels{96.0f};
    float bounds_padding_pixels{10.0f};
};

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

struct SPACEGAMEPRESENTATION_API FSoftTargetSelectionResult {
    FRegistryEntityHandle handle{};
    float range_alpha{0.0f};
    float indicator_radius_pixels{0.0f};
    float world_units_per_pixel{0.0f};
    bool previous_target_can_fade{false};
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
    select_soft_target(ml::entity_registry::EntityData::ConstView entities,
                       TConstArrayView<int> generations,
                       TConstArrayView<EEntityOverlayObjectiveRole> objective_roles,
                       FSoftTargetSelectionContext const& context,
                       FSoftTargetSelectionSettings const& settings,
                       FRegistryEntityHandle current_target) -> FSoftTargetSelectionResult;

[[nodiscard]] SPACEGAMEPRESENTATION_API auto
    collect_entity_overlay_instances(ml::entity_registry::EntityData::ConstView entities,
                                     TConstArrayView<EEntityOverlayObjectiveRole> objective_roles,
                                     FEntityOverlayTeamColours const& team_colours,
                                     FEntityOverlayHealthMaximums const& maximum_health,
                                     FVector3f origin,
                                     float maximum_range,
                                     TArray<FEntityOverlayInstance>& output_instances,
                                     FEntityOverlayCollector& collector)
        -> FEntityOverlayCollectionResult;
