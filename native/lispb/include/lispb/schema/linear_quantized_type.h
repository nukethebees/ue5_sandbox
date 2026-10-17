#pragma once

#include <codegen/schema/linear_quantized_schema.h>
#include <lispb/schema/resolved_type_ref.h>

#include <cstdint>

namespace lispb::schema {
struct LinearQuantizedType {
    ResolvedTypeRef source;
    std::uint32_t bit_width{};
    std::uint64_t reserved_codes{};
    codegen::QuantizationClipping clipping{codegen::QuantizationClipping::reject};
};
}
