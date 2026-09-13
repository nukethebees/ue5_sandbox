#pragma once

#include <sandbox/simulation/entities/DirectDamageEvents.h>
#include <sandbox/simulation/simulation/CollisionSystem.h>
#include <sandbox/simulation/simulation/LevelSimulationConfig.h>

struct FTestEntityRegistry;

namespace ml {
struct FOverlapHandler {
    FOverlapHandler(FTestEntityRegistry& registry, FOverlapResponseConfig const& config) noexcept;

    void handle(ioj::FDetectedOverlapsView overlaps);
  private:
    void append_damage(FRegistryEntityHandle entity);

    FTestEntityRegistry& registry_;
    std::int32_t damage_per_overlap_detection_{};
    DirectDamageEvents damage_events_;
};
}
