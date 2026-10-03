#include "ioj/sim/fighter_frame_spawn_queue.h"

namespace ioj::sim::fighters {
FrameSpawnQueue::FrameSpawnQueue(ml::FrameScratchResource& scratch_resource)
    : locations_{scratch_resource}
    , rotations_{scratch_resource}
    , teams_{&scratch_resource}
    , parents_{&scratch_resource}
    , targets_{&scratch_resource} {}
void FrameSpawnQueue::reserve(std::uint32_t const count) {
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
