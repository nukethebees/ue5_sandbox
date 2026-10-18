#pragma once

#include <codegen/schema/packed_integer_value.h>

#include <cstdint>

namespace ioj::layout {

struct IntegerVarintDistributionEntry {
    auto operator==(IntegerVarintDistributionEntry const&) const -> bool = default;

    lispb::codegen::PackedIntegerValue value{};
    std::uint64_t weight{};
};

}
