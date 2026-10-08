#pragma once

#include <cstdint>

namespace ioj::s7 {
struct Ratio {
    std::int64_t numerator{};
    std::int64_t denominator{1};
};
}
