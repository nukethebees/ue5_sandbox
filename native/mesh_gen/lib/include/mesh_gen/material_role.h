#pragma once

#include <cstdint>

namespace mesh_gen {

enum class MaterialRole : std::uint8_t {
    structure,
    armor,
    glass,
    emissive,
};

}
