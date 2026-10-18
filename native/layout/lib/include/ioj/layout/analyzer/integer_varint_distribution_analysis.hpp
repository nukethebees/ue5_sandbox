#pragma once

#include <ioj/layout/analyzer/integer_varint_distribution_entry_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct IntegerVarintDistributionAnalysis {
    lispb::schema::TypeId type{};
    std::uint64_t valid_entry_count{};
    std::optional<std::uint64_t> total_weight{0};
    std::optional<std::uint64_t> total_encoded_bytes{0};
    std::optional<long double> expected_bytes_per_value{};
    std::uint64_t selected_element_count{1};
    std::optional<long double> expected_selected_bytes{};
    std::vector<IntegerVarintDistributionEntryAnalysis> entries{};
    std::vector<Diagnostic> diagnostics{};
};

}
