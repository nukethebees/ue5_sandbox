#pragma once

#include <cstdint>

namespace mesh_gen {

enum class Shape : std::uint8_t {
    box,
    cylinder,
    sphere,
    cone,
    hex_tile,
    hex_frame,
    honeycomb_panel,
    beveled_box,
    wedge,
};

}
