#pragma once

#include <ioj/constrained_numbers.h>
#include <ioj/constrained_vectors.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/mesh_data.h>
#include <mesh_gen/vector_types.h>

#include <string_view>

namespace mesh_gen {

struct WedgeInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFiniteVector<float, 3>::valid(dimensions) ||
            !ioj::PositiveFinite<float>::valid(top_length)) {
            return "Wedge dimensions and top length must be finite and positive.";
        }
        if (!ioj::Finite<float>::valid(top_offset)) {
            return "Wedge top offset must be finite.";
        }
        auto const offset{top_offset < 0.f ? -top_offset : top_offset};
        if (top_length > dimensions.x() || offset + top_length * 0.5f > dimensions.x() * 0.5f) {
            return "Wedge top length and offset must keep the top within its base.";
        }
        return {};
    }

    Vec3f dimensions{100.0f, 100.0f, 50.0f};
    float top_length{50.0f};
    float top_offset{};
};
using WedgeParameters = CheckedParameters<WedgeInput>;

[[nodiscard]] auto generate_wedge(WedgeParameters const& parameters = {}) -> MeshData;

}
