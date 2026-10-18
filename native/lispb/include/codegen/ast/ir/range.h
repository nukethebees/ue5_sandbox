#pragma once

#include <cstdint>

namespace lispb::codegen::ir {

struct Range {
    std::uint32_t begin{};
    std::uint32_t count{};
};

} // namespace lispb::codegen::ir
