#include "sandbox/simulation/fighter_targeting.h"

#include "sandbox/core/vector_normalization.h"

#include <cassert>
#include <cstddef>

namespace ml::simulation::fighters {
auto select_opportunistic_target(Vector3f const location,
                                 Vector3f const aim_direction,
                                 std::span<FRegistryEntityHandle const> const candidates,
                                 EntityRegistryQueryView const registry,
                                 float const dot_threshold,
                                 float const squared_normal_tolerance) noexcept
    -> FRegistryEntityHandle {
    for (auto const candidate : candidates) {
        assert(candidate.index >= 0 && candidate.index < registry.num());
        assert(registry.generations[static_cast<std::size_t>(candidate.index)] ==
               candidate.generation);
        auto const direction{ml::native_math::safe_normal(
            registry.locations[candidate.index] - location, squared_normal_tolerance)};
        if (HMM_DotV3(aim_direction, direction) > dot_threshold) {
            return candidate;
        }
    }
    return {};
}
}
