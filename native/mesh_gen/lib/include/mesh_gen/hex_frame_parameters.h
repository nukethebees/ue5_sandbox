#pragma once

#include <ioj/constrained_numbers.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/hex_frame_uv_mode.h>
#include <mesh_gen/mesh_data.h>

#include <string_view>

namespace mesh_gen {

struct HexFrameInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(outer_radius) ||
            !ioj::PositiveFinite<float>::valid(wall_thickness) ||
            !ioj::PositiveFinite<float>::valid(depth)) {
            return "Hex-frame radius, wall thickness, and depth must be finite and positive.";
        }
        if (wall_thickness >= outer_radius) {
            return "Hex-frame wall thickness must be less than its outer radius.";
        }
        return uv_mode == HexFrameUvMode::per_face || uv_mode == HexFrameUvMode::perimeter
                 ? ""
                 : "Hex-frame UV mode is invalid.";
    }

    float outer_radius{50.0f};
    float wall_thickness{10.0f};
    float depth{20.0f};
    bool pointy_top{};
    HexFrameUvMode uv_mode{HexFrameUvMode::per_face};
};
using HexFrameParameters = CheckedParameters<HexFrameInput>;

[[nodiscard]] auto generate_hex_frame(HexFrameParameters const& parameters = {}) -> MeshData;

}
