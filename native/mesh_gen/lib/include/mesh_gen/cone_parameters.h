#pragma once

#include <ioj/constrained_numbers.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/mesh_data.h>

#include <cstdint>
#include <string_view>

namespace mesh_gen {

struct ConeInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(radius) ||
            !ioj::PositiveFinite<float>::valid(height)) {
            return "Cone radius and height must be finite and positive.";
        }
        return radial_segments >= 3
                 ? ""
                 : "Cone radius and height must be positive, with at least 3 segments.";
    }

    float radius{50.0f};
    float height{100.0f};
    std::int32_t radial_segments{32};
};
using ConeParameters = CheckedParameters<ConeInput>;

[[nodiscard]] auto generate_cone(ConeParameters const& parameters = {}) -> MeshData;

}
