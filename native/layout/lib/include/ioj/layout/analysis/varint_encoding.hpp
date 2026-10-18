#pragma once
#include <ioj/layout/analysis/row_range.hpp>

#include <codegen/schema/integer_varint_encoding.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct VarintEncoding {
    lispb::schema::TypeId source_type{};
    lispb::codegen::IntegerVarintEncoding encoding{
        lispb::codegen::IntegerVarintEncoding::unsigned_varint};
    std::uint32_t minimum_encoded_bytes{};
    std::uint32_t maximum_encoded_bytes{};
};

}
