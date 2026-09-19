#include "ioj/sim/fighter_frame_spawn_queue.h"

namespace ioj::sim::fighters {
FrameSpawnQueue::FrameSpawnQueue(ml::FrameScratch& scratch)
    : locations{scratch}
    , rotations{scratch}
    , teams{&scratch}
    , parents{&scratch}
    , targets{&scratch} {}
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
                          EntityUniqueId const parent,
                          EntityUniqueId const target) {
    locations.add(location);
    rotations.add(rotation);
    teams.add(team);
    parents.add(parent);
    targets.add(target);
}
auto FrameSpawnQueue::get_const_view() const -> FighterSpawnQueueConstView {
    return {locations.get_const_view(),
            rotations.get_const_view(),
            teams.view(),
            parents.view(),
            targets.view()};
}
}
