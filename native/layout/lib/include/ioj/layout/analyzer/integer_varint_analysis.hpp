#pragma once

#include <ioj/layout/diagnostic.hpp>

#include <codegen/schema/integer_varint_encoding.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct IntegerVarintAnalysis {
    lispb::schema::TypeId type{};
    lispb::schema::TypeId source_type{};
    lispb::codegen::IntegerVarintEncoding encoding{
        lispb::codegen::IntegerVarintEncoding::unsigned_varint};
    std::uint32_t minimum_encoded_bytes{};
    std::uint32_t maximum_encoded_bytes{};
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> minimum_total_bytes{};
    std::optional<std::uint64_t> maximum_total_bytes{};
    std::vector<Diagnostic> diagnostics{};
};

}
