#pragma once

#include <cstdint>

namespace ioj::layout {

enum class SoaAllocationStrategy : std::uint8_t {
    separate_columns,
    aligned_contiguous,
};

}
