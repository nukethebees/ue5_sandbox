#pragma once

#include <mesh_gen/mesh_data.h>

#include <cstdint>

namespace mesh_gen {

struct HoneycombPanelParameters {
    std::int32_t rows{3};
    std::int32_t columns{4};
    float cell_radius{50.0f};
    float wall_thickness{8.0f};
    float depth{20.0f};
    bool pointy_top{};
};

[[nodiscard]] auto generate_honeycomb_panel(HoneycombPanelParameters const& parameters = {})
    -> MeshData;

}
