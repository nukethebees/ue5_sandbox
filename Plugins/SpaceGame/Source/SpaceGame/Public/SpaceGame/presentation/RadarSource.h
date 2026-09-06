#pragma once

#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"
#include "SandboxUI/Radar/RadarTypes.h"
#include "SpaceGame/entities/TeamColours.h"
#include "SpaceGame/entities/TestEntityRegistryData.h"

#include "SandboxNative/RegistryEntityHandle.h"

struct SPACEGAME_API FRadarTeamColours {
    FTeamColours capital_ship;
    FTeamColours fighter;
    FTeamColours turret;
};

struct SPACEGAME_API FRadarCollectionResult {
    int32 candidate_count{0};
    int32 visible_count{0};
};

[[nodiscard]] SPACEGAME_API auto
    collect_radar_instances(ml::entity_registry::EntityData::ConstView entities,
                            TConstArrayView<int32> generations,
                            TConstArrayView<EEntityOverlayObjectiveRole> objective_roles,
                            FRadarTeamColours const& team_colours,
                            FTransform const& player_transform,
                            FRegistryEntityHandle player_handle,
                            FRegistryEntityHandle selected_handle,
                            float maximum_range,
                            TArray<FRadarInstance>& output_instances) -> FRadarCollectionResult;
