#pragma once

#include <ioj/constrained_numbers.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/mesh_data.h>

#include <cstdint>
#include <string_view>

namespace mesh_gen {

struct CylinderInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(radius) ||
            !ioj::PositiveFinite<float>::valid(height)) {
            return "Cylinder radius and height must be finite and positive.";
        }
        return radial_segments >= 3
                 ? ""
                 : "Cylinder radius and height must be positive, with at least 3 segments.";
    }

    float radius{50.0f};
    float height{100.0f};
    std::int32_t radial_segments{32};
};
using CylinderParameters = CheckedParameters<CylinderInput>;

[[nodiscard]] auto generate_cylinder(CylinderParameters const& parameters = {}) -> MeshData;

}
