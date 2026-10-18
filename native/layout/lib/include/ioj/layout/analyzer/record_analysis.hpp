#pragma once

#include <ioj/layout/analyzer/record_aggregate_analysis.hpp>
#include <ioj/layout/analyzer/record_member_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace ioj::layout {

struct RecordAnalysis {
    lispb::schema::TypeId type{};
    std::vector<RecordMemberAnalysis> members{};
    std::optional<std::uint64_t> payload_bytes{};
    std::optional<std::uint64_t> internal_padding_bytes{};
    std::optional<std::uint64_t> tail_padding_bytes{};
    std::optional<std::uint64_t> size_bytes{};
    std::optional<std::uint64_t> alignment_bytes{};
    std::vector<Diagnostic> diagnostics{};
    RecordAggregateAnalysis aggregate{};
};

}
