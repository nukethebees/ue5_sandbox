#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/constrained_vectors.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/mesh_data.h>
#include <mesh_gen/vector_types.h>

#include <string_view>

namespace mesh_gen {

struct BeveledBoxInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFiniteVector<float, 3>::valid(dimensions) ||
            !ioj::PositiveFinite<float>::valid(bevel_width)) {
            return "Beveled-box dimensions and bevel width must be finite and positive.";
        }
        for (auto const dimension : dimensions.elements) {
            if (bevel_width >= dimension * 0.5f) {
                return "Beveled-box bevel width must be less than half its smallest dimension.";
            }
        }
        return {};
    }

    Vec3f dimensions{100.0f, 100.0f, 100.0f};
    float bevel_width{10.0f};
};
using BeveledBoxParameters = CheckedParameters<BeveledBoxInput>;

[[nodiscard]] auto generate_beveled_box(BeveledBoxParameters const& parameters = {}) -> MeshData;

}
