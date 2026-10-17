#pragma once

#include <cstdint>
#include <string>

namespace lispb::schema {
struct PackedReservedBits {
    std::string name;
    std::uint32_t bit_width{};
};
}
