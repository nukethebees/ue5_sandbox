#pragma once

#include <ioj/layout/analyzer/enum_domain_analysis.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <optional>
#include <vector>

namespace ioj::layout {

struct EnumTargetComparison {
    EnumDomainAnalysis first{};
    EnumDomainAnalysis second{};
    std::optional<NumericDelta> backing_size_delta{};
    std::optional<NumericDelta> backing_alignment_delta{};
    std::optional<NumericDelta> backing_value_bit_delta{};
    std::optional<NumericDelta> backing_bit_delta{};
    std::optional<NumericDelta> unused_backing_code_delta{};
    std::optional<bool> backing_fit_changed{};
    std::optional<NumericDelta> total_storage_delta{};
    std::optional<NumericDelta> cache_line_size_delta{};
    std::optional<NumericDelta> minimum_cache_line_delta{};
    std::optional<NumericDelta> complete_elements_per_cache_line_delta{};
    std::optional<NumericDelta> cache_line_straddling_delta{};
    std::optional<NumericDelta> page_size_delta{};
    std::optional<NumericDelta> minimum_page_delta{};
    std::optional<NumericDelta> complete_elements_per_page_delta{};
    std::optional<NumericDelta> page_straddling_delta{};
    std::vector<Diagnostic> diagnostics{};
};

}
