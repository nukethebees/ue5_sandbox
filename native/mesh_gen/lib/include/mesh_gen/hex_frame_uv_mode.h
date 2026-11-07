#pragma once

#include <cstdint>

namespace mesh_gen {

enum class HexFrameUvMode : std::uint8_t {
    per_face,
    perimeter,
};

}
