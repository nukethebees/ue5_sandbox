#pragma once

#include <cstdint>

namespace ioj::s7 {
struct AstLimits {
    std::uint32_t max_depth{256};
    std::uint32_t max_nodes{1'000'000};
    std::uint32_t max_child_indices{2'000'000};
    std::uint32_t max_text_bytes{64 * 1024 * 1024};
};
}
