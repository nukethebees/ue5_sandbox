#pragma once

#include <ioj/s7/node_kind.h>

#include <cstdint>

namespace ioj::s7 {
struct Node {
    // Index the matching payload array; lists and text use contiguous ranges, scalars count one.
    NodeKind kind{NodeKind::List};
    std::uint32_t offset{};
    std::uint32_t count{};
};
}
