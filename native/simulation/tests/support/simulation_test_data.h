#pragma once
#include <ioj/sim/entity_registry.h>
#include <vector>

namespace ioj::sim::tests {
inline auto copy_vectors(Vectors3fConstView source) -> std::vector<Vector3f> {
    std::vector<Vector3f> result{};
    result.reserve(source.num());
    for (std::int32_t i{}; i < source.num(); ++i) {
        result.push_back(source[i]);
    }
    return result;
}
inline auto copy_handles(RegistryEntityHandles const& source) -> std::vector<RegistryEntityHandle> {
    std::vector<RegistryEntityHandle> result{};
    result.reserve(source.num());
    for (std::int32_t i{}; i < source.num(); ++i) {
        result.push_back(RegistryEntityHandle{source.registry_indices[i], source.generations[i]});
    }
    return result;
}
}
