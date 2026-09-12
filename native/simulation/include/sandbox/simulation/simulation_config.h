#pragma once

#include <cstdint>

namespace ml::simulation {
struct LaserWeaponConfig {
    std::int32_t damage{5};
    float projectile_speed{10000.f};
    float max_distance{10000.f};
    float fire_cooldown{0.33f};
};

struct LaserSimulationConfig {
    std::int32_t n_preallocated_instances{5000};
    std::int32_t collision_jobs{8};
};

struct OverlapResponseConfig {
    std::int32_t damage_per_overlap_detection{50};
};
}
