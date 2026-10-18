#pragma once

#include <ioj/layout/analyzer/exact_code_count.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct OptionalSentinelAnalysis {
    lispb::schema::TypeId type{};
    lispb::schema::TypeId source_type{};
    std::string sentinel_name{};
    lispb::codegen::PackedIntegerValue sentinel_value{};
    bool source_signedness{};
    lispb::codegen::PackedIntegerValue source_minimum{};
    lispb::codegen::PackedIntegerValue source_maximum{};
    std::optional<std::uint64_t> present_value_count{};
    std::uint64_t absence_code_count{1};
    std::uint64_t other_sentinel_code_count{};
    std::optional<std::uint64_t> unused_code_count{};
    std::uint32_t encoded_storage_bits{};
    ExactCodeCount total_code_count{};
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_encoded_bits{};
    std::vector<Diagnostic> diagnostics{};
};

}
