#pragma once
#include <SpaceGameSimulation/entities/TestEntityType.h>
#include <SpaceGameSimulation/entities/TestTeam.h>

struct FLaserSource {
    ETestTeam team{ETestTeam::White};
    ETestEntityType type{ETestEntityType::TubeSpinner};
    bool operator==(FLaserSource const&) const = default;
};
