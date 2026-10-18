#pragma once

#include <cstdint>

namespace ioj::layout {

enum class AccessOperation : std::uint8_t {
    read,
    write,
    read_write,
};

}
