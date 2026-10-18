#pragma once

#include <codegen/schema/packed_integer_value.h>

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct IntegerVarintDistributionEntryAnalysis {
    lispb::codegen::PackedIntegerValue value{};
    std::uint64_t weight{};
    std::uint32_t encoded_bytes{};
    std::optional<std::uint64_t> weighted_encoded_bytes{};
};

}
