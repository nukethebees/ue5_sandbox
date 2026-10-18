#pragma once

#include <ioj/layout/analyzer/tagged_union_distribution_entry_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct TaggedUnionDistributionAnalysis {
    lispb::schema::TypeId type{};
    std::uint64_t valid_entry_count{};
    std::optional<std::uint64_t> total_weight{0};
    std::optional<std::uint64_t> total_payload_extent_bytes{0};
    std::optional<std::uint64_t> total_payload_slack_bytes{0};
    std::optional<long double> expected_payload_extent_bytes_per_value{};
    std::optional<long double> expected_payload_slack_bytes_per_value{};
    std::uint64_t selected_element_count{1};
    std::optional<long double> expected_selected_payload_extent_bytes{};
    std::optional<long double> expected_selected_payload_slack_bytes{};
    std::vector<TaggedUnionDistributionEntryAnalysis> entries{};
    std::vector<Diagnostic> diagnostics{};
};

}
