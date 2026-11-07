#pragma once

#include <ioj/constrained_numbers.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/mesh_data.h>

#include <string_view>

namespace mesh_gen {

struct HexTileInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (!ioj::PositiveFinite<float>::valid(outer_radius) ||
            !ioj::PositiveFinite<float>::valid(depth) ||
            !ioj::PositiveFinite<float>::valid(bevel_width)) {
            return "Hex-tile radius, depth, and bevel width must be finite and positive.";
        }
        return bevel_width < outer_radius && bevel_width < depth * 0.5f
                 ? ""
                 : "Hex-tile bevel width must be less than its radius and half-depth.";
    }

    float outer_radius{50.0f};
    float depth{20.0f};
    float bevel_width{5.0f};
    bool pointy_top{};
};
using HexTileParameters = CheckedParameters<HexTileInput>;

[[nodiscard]] auto generate_hex_tile(HexTileParameters const& parameters = {}) -> MeshData;

}
