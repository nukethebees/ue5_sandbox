#pragma once

#include "SpaceGame/entities/TestEntityRegistryData.h"

#include "SandboxUI/EntityOverlay/EntityOverlayTypes.h"

struct SPACEGAME_API FEntityOverlayHealthMaximums {
    int32 capital_ship{1};
    int32 fighter{1};
    int32 turret{1};
};

struct SPACEGAME_API FEntityOverlayCollectionResult {
    int32 candidate_count{0};
    int32 invalid_health_count{0};
};

[[nodiscard]] SPACEGAME_API auto
    collect_entity_overlay_instances(ml::entity_registry::EntityData::ConstView entities,
                                     FEntityOverlayHealthMaximums const& maximum_health,
                                     FVector3f origin,
                                     float maximum_range,
                                     TArray<FEntityOverlayInstance>& output_instances,
                                     FEntityOverlayCollector& collector)
        -> FEntityOverlayCollectionResult;
