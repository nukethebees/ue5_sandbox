#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class NameOrigin : std::uint8_t {
    enclosing,
    global,
};

}
