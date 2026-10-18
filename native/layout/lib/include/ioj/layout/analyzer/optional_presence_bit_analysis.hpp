#pragma once

#include <ioj/layout/diagnostic.hpp>

#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct OptionalPresenceBitAnalysis {
    lispb::schema::TypeId type{};
    lispb::schema::TypeId source_type{};
    bool source_signedness{};
    lispb::codegen::PackedIntegerValue source_minimum{};
    lispb::codegen::PackedIntegerValue source_maximum{};
    std::optional<std::uint64_t> present_value_count{};
    std::uint64_t canonical_absence_state_count{1};
    std::uint64_t source_sentinel_code_count{};
    std::optional<std::uint64_t> source_unused_payload_codes{};
    std::uint32_t presence_bits{1};
    std::uint32_t payload_bits{};
    std::uint32_t encoded_storage_bits{};
    std::uint64_t noncanonical_absence_patterns{};
    std::uint64_t element_count{1};
    std::optional<std::uint64_t> total_encoded_bits{};
    std::vector<Diagnostic> diagnostics{};
};

}
