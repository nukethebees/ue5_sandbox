#pragma once

#include <SpaceGameSimulation/entities/DirectDamageEvents.h>
#include <SpaceGameSimulation/simulation/CollisionSystem.h>
#include <SpaceGameSimulation/simulation/LevelSimulationConfig.h>

struct FTestEntityRegistry;

namespace ml {
struct SPACEGAMESIMULATION_API FOverlapHandler {
    FOverlapHandler(FTestEntityRegistry& registry, FOverlapResponseConfig const& config) noexcept;

    void handle(ioj::FDetectedOverlapsView overlaps);
  private:
    void append_damage(FRegistryEntityHandle entity);

    FTestEntityRegistry& registry_;
    int32 damage_per_overlap_detection_{};
    DirectDamageEvents damage_events_;
};
}
