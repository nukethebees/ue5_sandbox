#pragma once

#include <ioj/layout/analyzer/optional_encoding_kind.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>

namespace ioj::layout {

struct OptionalEncodingSummary {
    lispb::schema::TypeId type{};
    lispb::schema::TypeId source_type{};
    OptionalEncodingKind kind{OptionalEncodingKind::sentinel};
    std::optional<std::string> absence_sentinel_name{};
    std::optional<std::uint64_t> present_value_count{};
    std::uint64_t canonical_absence_state_count{1};
    std::uint64_t source_sentinel_code_count{};
    std::uint64_t source_sentinel_codes_used_for_absence{};
    std::uint64_t remaining_source_sentinel_code_count{};
    std::optional<std::uint64_t> source_unused_payload_codes{};
    std::uint64_t noncanonical_absence_patterns{};
    std::uint32_t encoded_storage_bits{};
    std::optional<std::uint64_t> total_encoded_bits{};
};

}
