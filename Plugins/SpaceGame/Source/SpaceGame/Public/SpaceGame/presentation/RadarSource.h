#pragma once

#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"
#include "SandboxUI/Radar/RadarTypes.h"
#include "SpaceGame/entities/TestEntityRegistryData.h"

#include "SandboxNative/RegistryEntityHandle.h"

struct SPACEGAME_API FRadarContactColours {
    FLinearColor friendly{0.12f, 0.72f, 1.0f, 1.0f};
    FLinearColor hostile{1.0f, 0.08f, 0.035f, 1.0f};
    FLinearColor neutral{0.78f, 0.84f, 0.86f, 1.0f};
};

struct SPACEGAME_API FRadarCollectionResult {
    int32 candidate_count{0};
    int32 visible_count{0};
};

[[nodiscard]] SPACEGAME_API auto
    collect_radar_instances(ml::entity_registry::EntityData::ConstView entities,
                            TConstArrayView<int32> generations,
                            TConstArrayView<EEntityOverlayObjectiveRole> objective_roles,
                            FRadarContactColours const& contact_colours,
                            FTransform const& player_transform,
                            FRegistryEntityHandle player_handle,
                            FRegistryEntityHandle selected_handle,
                            ETestTeam player_team,
                            bool automatic_range,
                            float minimum_range,
                            float maximum_range,
                            TArray<FRadarInstance>& output_instances) -> FRadarCollectionResult;
