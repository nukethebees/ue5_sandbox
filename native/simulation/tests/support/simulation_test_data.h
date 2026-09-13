#pragma once
#include <sandbox/simulation/entities/TestEntityRegistry.h>
#include <vector>

namespace ml::simulation_tests {
inline auto copy_vectors(ml::simulation::Vectors3fConstView source)
    -> std::vector<ml::simulation::Vector3f> {
    std::vector<ml::simulation::Vector3f> result{};
    result.reserve(source.num());
    for (std::int32_t i{}; i < source.num(); ++i) {
        result.push_back(source[i]);
    }
    return result;
}
inline auto copy_handles(RegistryEntityHandles const& source)
    -> std::vector<FRegistryEntityHandle> {
    std::vector<FRegistryEntityHandle> result{};
    result.reserve(source.num());
    for (std::int32_t i{}; i < source.num(); ++i) {
        result.push_back(FRegistryEntityHandle{source.registry_indices[i], source.generations[i]});
    }
    return result;
}
}
