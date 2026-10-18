#pragma once

#include <ioj/layout/analyzer/tagged_union_aggregate_analysis.hpp>
#include <ioj/layout/analyzer/tagged_union_alternative_analysis.hpp>
#include <ioj/layout/diagnostic.hpp>
#include <ioj/layout/type_facts.hpp>

#include <lispb/schema/indices.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ioj::layout {

struct TaggedUnionAnalysis {
    lispb::schema::TypeId type{};
    lispb::schema::TypeId discriminant_type{};
    std::optional<TypeFacts> discriminant_facts{};
    std::vector<TaggedUnionAlternativeAnalysis> alternatives{};
    std::optional<std::uint64_t> largest_alternative_bytes{};
    std::optional<std::uint64_t> payload_size_bytes{};
    std::optional<std::uint64_t> payload_alignment_bytes{};
    std::optional<std::uint64_t> payload_offset_bytes{};
    std::optional<std::uint64_t> internal_padding_bytes{};
    std::optional<std::uint64_t> tail_padding_bytes{};
    std::optional<std::uint64_t> size_bytes{};
    std::optional<std::uint64_t> alignment_bytes{};
    std::vector<std::string> mapped_live_tags{};
    std::vector<std::string> unmapped_live_tags{};
    std::vector<std::string> sentinel_tags{};
    std::optional<std::string> count_sentinel_tag{};
    std::vector<Diagnostic> diagnostics{};
    TaggedUnionAggregateAnalysis aggregate{};
};

}
