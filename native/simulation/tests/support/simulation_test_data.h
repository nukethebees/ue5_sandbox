#pragma once
#include <ioj/sim/column_math.h>
#include <ioj/sim/vectors3f.h>
#include <vector>

namespace ioj::sim::tests {
inline auto copy_vectors(VectorColumns auto const& source) -> std::vector<Vector3f> {
    std::vector<Vector3f> result{};
    result.reserve(source.num());
    for (std::int32_t i{}; i < source.num(); ++i) {
        result.push_back(vector_at(source, i));
    }
    return result;
}
}
