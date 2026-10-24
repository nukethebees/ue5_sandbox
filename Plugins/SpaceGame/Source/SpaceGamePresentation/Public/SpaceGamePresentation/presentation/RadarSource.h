#pragma once
#include "ioj/sim/entities/entity_id.h"
#include "SpaceGamePresentation/presentation/AgentDisplayBatch.h"
#include "SpaceGamePresentation/presentation/LevelPresentationSettings.h"
#include <SpaceGamePresentation/entities/TestTeam.h>

#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"
#include "SandboxUI/Radar/RadarTypes.h"

struct SPACEGAMEPRESENTATION_API FRadarContactColours {
    FLinearColor friendly{0.12f, 0.72f, 1.0f, 1.0f};
    FLinearColor hostile{1.0f, 0.08f, 0.035f, 1.0f};
    FLinearColor neutral{0.78f, 0.84f, 0.86f, 1.0f};
};

struct SPACEGAMEPRESENTATION_API FRadarCollectionResult {
    int32 candidate_count{0};
    int32 visible_count{0};
};

[[nodiscard]] SPACEGAMEPRESENTATION_API auto sanitize_radar_settings(ioj::settings::Radar settings)
    -> ioj::settings::Radar;

namespace ioj::radar_source {
[[nodiscard]] SPACEGAMEPRESENTATION_API auto
    to_radar_display_radius(float world_distance, ioj::settings::Radar const& settings) -> float;
[[nodiscard]] SPACEGAMEPRESENTATION_API auto to_radar_position(FVector3f local_delta,
                                                               ioj::settings::Radar const& settings)
    -> FVector3f;
}

[[nodiscard]] SPACEGAMEPRESENTATION_API auto
    collect_radar_instances(std::span<::ioj::presentation::AgentDisplayBatch const> batches,
                            TConstArrayView<EEntityOverlayObjectiveRole> objective_roles,
                            FRadarContactColours const& contact_colours,
                            FTransform const& player_transform,
                            ::ioj::sim::EntityId player_id,
                            ETestTeam player_team,
                            ioj::settings::Radar const& settings,
                            FRadarFrame& output_frame) -> FRadarCollectionResult;
