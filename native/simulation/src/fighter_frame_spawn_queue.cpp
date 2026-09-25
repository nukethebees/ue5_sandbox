#include "ioj/sim/fighter_frame_spawn_queue.h"

namespace ioj::sim::fighters {
FrameSpawnQueue::FrameSpawnQueue(ml::FrameScratch& scratch)
    : locations_{scratch}
    , rotations_{scratch}
    , teams_{&scratch}
    , parents_{&scratch}
    , targets_{&scratch} {}
void FrameSpawnQueue::reserve(std::int32_t const count) {
    locations_.reserve(count);
    rotations_.reserve(count);
    teams_.reserve(count);
    parents_.reserve(count);
    targets_.reserve(count);
}
void FrameSpawnQueue::clear() {
    locations_.clear();
    rotations_.clear();
    teams_.clear();
    parents_.clear();
    targets_.clear();
}
void FrameSpawnQueue::add(Vector3f const location,
                          Rotator3f const rotation,
                          Team const team,
                          EntityUniqueId const parent,
                          EntityUniqueId const target) {
    locations_.add(location);
    rotations_.add(rotation);
    teams_.add(team);
    parents_.add(parent);
    targets_.add(target);
}
}
