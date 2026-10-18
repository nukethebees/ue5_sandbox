#pragma once

#include <codegen/schema/packed_integer_value.h>

#include <cstdint>

namespace ioj::layout {

struct IntegerVarintDistributionEntry {
    lispb::codegen::PackedIntegerValue value{};
    std::uint64_t weight{};
};

}
