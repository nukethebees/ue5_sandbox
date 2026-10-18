#pragma once

#include <cstdint>

namespace lispb::codegen {

enum class IncludeStyle : std::uint8_t {
    inferred,
    quoted,
    system,
};

}
