#pragma once

#include <ioj/layout/analyzer/union_aggregate_analysis.hpp>
#include <ioj/layout/analyzer/union_alternative_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct UnionAnalysis {
    lispb::schema::TypeId type{};
    std::vector<UnionAlternativeAnalysis> alternatives{};
    std::optional<std::uint64_t> largest_alternative_bytes{};
    std::optional<std::uint64_t> tail_padding_bytes{};
    std::optional<std::uint64_t> size_bytes{};
    std::optional<std::uint64_t> alignment_bytes{};
    std::vector<Diagnostic> diagnostics{};
    UnionAggregateAnalysis aggregate{};
};

}
