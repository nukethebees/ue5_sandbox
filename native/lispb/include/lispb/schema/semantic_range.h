#pragma once

#include <cstdint>

namespace lispb::schema {

struct SemanticRange {
    std::uint32_t begin{};
    std::uint32_t count{};
};

} // namespace lispb::schema
