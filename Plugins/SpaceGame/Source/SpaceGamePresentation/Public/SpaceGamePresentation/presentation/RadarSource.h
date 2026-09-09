#pragma once

#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"
#include "SandboxUI/Radar/RadarTypes.h"
#include "SpaceGamePresentation/presentation/LevelPresentationSettings.h"
#include "SpaceGameSimulation/entities/TestEntityRegistryData.h"

#include "SandboxNative/RegistryEntityHandle.h"

struct SPACEGAMEPRESENTATION_API FRadarContactColours {
    FLinearColor friendly{0.12f, 0.72f, 1.0f, 1.0f};
    FLinearColor hostile{1.0f, 0.08f, 0.035f, 1.0f};
    FLinearColor neutral{0.78f, 0.84f, 0.86f, 1.0f};
};

struct SPACEGAMEPRESENTATION_API FRadarCollectionResult {
    int32 candidate_count{0};
    int32 visible_count{0};
};

[[nodiscard]] SPACEGAMEPRESENTATION_API auto sanitize_radar_settings(FRadarSettings settings)
    -> FRadarSettings;

namespace ml::radar_source {
[[nodiscard]] SPACEGAMEPRESENTATION_API auto to_radar_display_radius(float world_distance,
                                                                     FRadarSettings const& settings)
    -> float;
[[nodiscard]] SPACEGAMEPRESENTATION_API auto to_radar_position(FVector3f local_delta,
                                                               FRadarSettings const& settings)
    -> FVector3f;
}

[[nodiscard]] SPACEGAMEPRESENTATION_API auto
    collect_radar_instances(ml::entity_registry::EntityData::ConstView entities,
                            TConstArrayView<int32> generations,
                            TConstArrayView<EEntityOverlayObjectiveRole> objective_roles,
                            FRadarContactColours const& contact_colours,
                            FTransform const& player_transform,
                            FRegistryEntityHandle player_handle,
                            FRegistryEntityHandle selected_handle,
                            ETestTeam player_team,
                            FRadarSettings const& settings,
                            FRadarFrame& output_frame) -> FRadarCollectionResult;
