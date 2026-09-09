#include "SpaceGame/simulation/CollisionSystem.h"

namespace ml::ioj {
void FCollisionSystem::initialise(FEntityAABBs const& bounds) {
    entity_aabbs_ = bounds;
}
void FCollisionSystem::update() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::update);
    rebuild_grid();
}
FCollisionSystem::FCollisionSystem(FTestEntityRegistry const& registry) noexcept
    : uniform_grid_{registry} {}
void FCollisionSystem::rebuild_grid() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::rebuild_grid);
    uniform_grid_.rebuild_grid(entity_aabbs_);
}
}
