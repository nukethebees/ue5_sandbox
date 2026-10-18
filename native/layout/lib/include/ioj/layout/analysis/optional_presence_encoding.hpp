#pragma once
#include <ioj/layout/analysis/row_range.hpp>

#include <codegen/schema/packed_integer_value.h>
#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct OptionalPresenceEncoding {
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
};

}
