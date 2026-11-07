#pragma once

#include <ioj/constrained_numbers.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/mesh_data.h>

#include <cstdint>
#include <string_view>

namespace mesh_gen {

struct SphereInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(radius)) {
            return "Sphere radius must be finite and positive.";
        }
        return longitude_segments >= 3 && latitude_segments >= 2
                 ? ""
                 : "Sphere radius must be positive, with at least 3 longitude and 2 latitude "
                   "segments.";
    }

    float radius{50.0f};
    std::int32_t longitude_segments{32};
    std::int32_t latitude_segments{16};
};
using SphereParameters = CheckedParameters<SphereInput>;

[[nodiscard]] auto generate_sphere(SphereParameters const& parameters = {}) -> MeshData;

}
