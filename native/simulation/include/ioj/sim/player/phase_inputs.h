#pragma once

namespace ioj {
class FrameMemoryResource;
}

namespace ioj::sim {
class DamageQueue;
}

namespace ioj::sim::lasers {
struct Sim;
}

namespace ioj::sim::player {
struct ActionInputs {
    lasers::Sim& laser_simulation;
};

struct ResolutionInputs {
    DamageQueue const& damage_queue;
};
} // namespace ioj::sim::player
