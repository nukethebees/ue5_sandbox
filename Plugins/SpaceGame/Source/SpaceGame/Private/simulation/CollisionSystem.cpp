#include "SpaceGame/simulation/CollisionSystem.h"

namespace ml::ioj {
void FCollisionSystem::initialise(FEntityAABBs const& bounds) {
    entity_aabbs_ = bounds;
}
void FCollisionSystem::update() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::update);
    rebuild_grid();
}
void FCollisionSystem::set_entity_registry(FTestEntityRegistry const& registry) {
    entity_registry_ = &registry;
    uniform_grid_.set_entity_registry(registry);
}
void FCollisionSystem::rebuild_grid() {
    TRACE_CPUPROFILER_EVENT_SCOPE(Sandbox::FCollisionSystem::rebuild_grid);
    uniform_grid_.rebuild_grid(entity_aabbs_);
}
}
