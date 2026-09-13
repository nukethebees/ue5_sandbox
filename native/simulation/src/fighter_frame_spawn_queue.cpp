#include "sandbox/simulation/fighter_frame_spawn_queue.h"

namespace ml::simulation::fighters {
FrameSpawnQueue::FrameSpawnQueue(std::pmr::memory_resource* const resource)
    : locations{resource}
    , rotations{resource}
    , teams{resource}
    , parents{resource}
    , targets{resource} {}
void FrameSpawnQueue::reserve(std::int32_t const count) {
    locations.reserve(count);
    rotations.reserve(count);
    teams.reserve(count);
    parents.reserve(count);
    targets.reserve(count);
}
void FrameSpawnQueue::clear() {
    locations.clear();
    rotations.clear();
    teams.clear();
    parents.clear();
    targets.clear();
}
void FrameSpawnQueue::add(Vector3f const location,
                          Rotator3f const rotation,
                          Team const team,
                          FRegistryEntityHandle const parent,
                          FRegistryEntityHandle const target) {
    locations.add(location);
    rotations.add(rotation);
    teams.add(team);
    parents.add(parent);
    targets.add(target);
}
auto FrameSpawnQueue::get_const_view() const -> TestCapitalShipFighterSpawnQueueConstView {
    return {locations.get_const_view(),
            rotations.get_const_view(),
            teams.view(),
            parents.view(),
            targets.view()};
}
}
