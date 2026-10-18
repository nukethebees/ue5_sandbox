#pragma once
#include <cstdint>

namespace lispb::codegen::sexpr::output_detail {
struct Range {
    std::uint32_t begin{};
    std::uint32_t count{};
};
}
