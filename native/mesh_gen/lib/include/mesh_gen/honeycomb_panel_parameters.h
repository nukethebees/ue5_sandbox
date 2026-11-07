#pragma once

#include <ioj/constrained_numbers.h>

#include <mesh_gen/checked_parameters.h>
#include <mesh_gen/mesh_data.h>

#include <cstdint>
#include <string_view>

namespace mesh_gen {

struct HoneycombPanelInput {
    [[nodiscard]] constexpr auto error() const -> std::string_view {
        if (rows <= 0 || columns <= 0 || !ioj::PositiveFinite<float>::valid(cell_radius) ||
            !ioj::PositiveFinite<float>::valid(wall_thickness) ||
            !ioj::PositiveFinite<float>::valid(depth)) {
            return "Honeycomb rows and columns must be positive; radius, wall thickness, and depth "
                   "must be finite and positive.";
        }
        return wall_thickness < cell_radius
                 ? ""
                 : "Honeycomb wall thickness must be less than its cell radius.";
    }

    std::int32_t rows{3};
    std::int32_t columns{4};
    float cell_radius{50.0f};
    float wall_thickness{8.0f};
    float depth{20.0f};
    bool pointy_top{};
};
using HoneycombPanelParameters = CheckedParameters<HoneycombPanelInput>;

[[nodiscard]] auto generate_honeycomb_panel(HoneycombPanelParameters const& parameters = {})
    -> MeshData;

}
