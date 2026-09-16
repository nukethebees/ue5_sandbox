#pragma once
#include <ioj/sim/vectors3f.h>
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
}
