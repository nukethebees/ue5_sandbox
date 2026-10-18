#pragma once

#include <cstdint>

namespace lispb::codegen {

struct CppTypeDependencyRange {
    std::uint32_t begin{};
    std::uint32_t count{};
};

} // namespace lispb::codegen
