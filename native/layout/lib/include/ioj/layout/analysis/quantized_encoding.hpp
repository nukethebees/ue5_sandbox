#pragma once
#include <ioj/layout/analysis/row_range.hpp>
#include <ioj/layout/analyzer/exact_code_count.hpp>

#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/quantization_clipping.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>

namespace ioj::layout {

struct QuantizedEncoding {
    lispb::schema::TypeId source_type{};
    lispb::codegen::PackedIntegerValue source_minimum{};
    lispb::codegen::PackedIntegerValue source_maximum{};
    std::optional<std::uint64_t> source_span{};
    std::uint32_t encoded_storage_bits{};
    ExactCodeCount total_code_count{};
    std::uint64_t reserved_code_count{};
    ExactCodeCount usable_code_count{};
    long double resolution{};
    long double maximum_rounding_error{};
    bool minimum_endpoint_exact{};
    bool maximum_endpoint_exact{};
    lispb::codegen::QuantizationClipping clipping{lispb::codegen::QuantizationClipping::reject};
};

}
