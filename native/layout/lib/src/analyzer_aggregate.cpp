#include "analyzer_internal.hpp"

namespace ioj::layout {

auto Analyzer::analyze_record(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const type,
                              AbiProfile const& abi,
                              std::uint64_t const element_count) -> RecordAnalysis {
    auto result{PhysicalFactsResolver{types, abi}.analyze_record(type)};
    result.aggregate.element_count = element_count;

    auto scale = [&](std::optional<std::uint64_t> const value,
                     std::string const& description) -> std::optional<std::uint64_t> {
        if (!value.has_value()) {
            return std::nullopt;
        }
        auto const scaled{checked_multiply(*value, element_count)};
        if (!scaled.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Record aggregate " + description + " overflows uint64."});
        }
        return scaled;
    };
    result.aggregate.total_storage_bytes = scale(result.size_bytes, "storage byte count");
    result.aggregate.total_payload_bytes = scale(result.payload_bytes, "payload byte count");
    result.aggregate.total_internal_padding_bytes =
        scale(result.internal_padding_bytes, "internal-padding byte count");
    result.aggregate.total_tail_padding_bytes =
        scale(result.tail_padding_bytes, "tail-padding byte count");
    if (result.aggregate.total_internal_padding_bytes.has_value() &&
        result.aggregate.total_tail_padding_bytes.has_value()) {
        result.aggregate.total_padding_bytes =
            checked_add(*result.aggregate.total_internal_padding_bytes,
                        *result.aggregate.total_tail_padding_bytes);
        if (!result.aggregate.total_padding_bytes.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Record aggregate padding byte count overflows uint64."});
        }
    }
    result.aggregate.cache_capacity =
        cache_capacity_analysis(result.aggregate.total_storage_bytes, abi.memory_facts());

    if (result.size_bytes.has_value() && *result.size_bytes != 0) {
        auto const& memory{abi.memory_facts()};
        result.aggregate.cache_line_bytes = memory.cache_line_bytes;
        if (!memory.cache_line_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Cache-line size is unknown for ABI profile '" + abi.name() + "'."});
        } else if (*memory.cache_line_bytes == 0) {
            result.aggregate.cache_line_bytes.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "ABI profile cache-line size must be non-zero."});
        } else {
            if (result.aggregate.total_storage_bytes.has_value()) {
                result.aggregate.minimum_cache_lines = minimum_regions(
                    *result.aggregate.total_storage_bytes, *memory.cache_line_bytes);
            }
            result.aggregate.complete_elements_per_cache_line =
                *memory.cache_line_bytes / *result.size_bytes;
            result.aggregate.cache_line_straddling_elements =
                straddling_elements(*result.size_bytes,
                                    element_count,
                                    *memory.cache_line_bytes,
                                    result.aggregate.total_storage_bytes);
        }

        result.aggregate.page_bytes = memory.page_bytes;
        if (!memory.page_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Page size is unknown for ABI profile '" + abi.name() + "'."});
        } else if (*memory.page_bytes == 0) {
            result.aggregate.page_bytes.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "ABI profile page size must be non-zero."});
        } else {
            if (result.aggregate.total_storage_bytes.has_value()) {
                result.aggregate.minimum_pages =
                    minimum_regions(*result.aggregate.total_storage_bytes, *memory.page_bytes);
            }
            result.aggregate.complete_elements_per_page = *memory.page_bytes / *result.size_bytes;
            result.aggregate.page_straddling_elements =
                straddling_elements(*result.size_bytes,
                                    element_count,
                                    *memory.page_bytes,
                                    result.aggregate.total_storage_bytes);
        }
    }
    return result;
}

auto Analyzer::compare_record_targets(RecordAnalysis const& first, RecordAnalysis const& second)
    -> RecordTargetComparison {
    RecordTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target record: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target record: " + diagnostic.message});
    }
    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Compared target analyses describe different records."});
        return result;
    }
    if (first.aggregate.element_count != second.aggregate.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target record analyses use different element counts."});
        return result;
    }

    std::set<std::string, std::less<>> expected_names;
    for (auto const& member : first.members) {
        expected_names.insert(member.name);
    }
    std::set<std::string, std::less<>> second_names;
    for (auto const& member : second.members) {
        second_names.insert(member.name);
    }
    if (expected_names.size() != first.members.size() ||
        second_names.size() != second.members.size() || expected_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target record analyses do not contain the same unique members."});
        return result;
    }

    for (auto const& first_member : first.members) {
        auto const second_member{
            std::ranges::find(second.members, first_member.name, &RecordMemberAnalysis::name)};
        if (second_member == second.members.end() ||
            first_member.semantic_type != second_member->semantic_type ||
            first_member.element_count != second_member->element_count) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target record member '" + first_member.name +
                     "' has incompatible semantic identity or element count."});
            return result;
        }
    }

    for (auto const& first_member : first.members) {
        auto const second_member{
            std::ranges::find(second.members, first_member.name, &RecordMemberAnalysis::name)};
        auto const first_size{first_member.element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_size{second_member->element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_alignment{first_member.element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_alignment{second_member->element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        result.members.push_back(
            {.name = first_member.name,
             .first = first_member,
             .second = *second_member,
             .element_size_delta = numeric_delta(first_size, second_size),
             .element_alignment_delta = numeric_delta(first_alignment, second_alignment),
             .offset_delta = numeric_delta(first_member.offset_bytes, second_member->offset_bytes),
             .extent_delta = numeric_delta(first_member.extent_bytes, second_member->extent_bytes),
             .padding_before_delta = numeric_delta(first_member.padding_before_bytes,
                                                   second_member->padding_before_bytes)});
    }

    result.payload_delta = numeric_delta(first.payload_bytes, second.payload_bytes);
    result.internal_padding_delta =
        numeric_delta(first.internal_padding_bytes, second.internal_padding_bytes);
    result.tail_padding_delta = numeric_delta(first.tail_padding_bytes, second.tail_padding_bytes);
    result.size_delta = numeric_delta(first.size_bytes, second.size_bytes);
    result.alignment_delta = numeric_delta(first.alignment_bytes, second.alignment_bytes);
    result.total_storage_delta =
        numeric_delta(first.aggregate.total_storage_bytes, second.aggregate.total_storage_bytes);
    result.total_payload_delta =
        numeric_delta(first.aggregate.total_payload_bytes, second.aggregate.total_payload_bytes);
    result.total_internal_padding_delta =
        numeric_delta(first.aggregate.total_internal_padding_bytes,
                      second.aggregate.total_internal_padding_bytes);
    result.total_tail_padding_delta = numeric_delta(first.aggregate.total_tail_padding_bytes,
                                                    second.aggregate.total_tail_padding_bytes);
    result.total_padding_delta =
        numeric_delta(first.aggregate.total_padding_bytes, second.aggregate.total_padding_bytes);
    result.cache_line_size_delta =
        numeric_delta(first.aggregate.cache_line_bytes, second.aggregate.cache_line_bytes);
    result.minimum_cache_line_delta =
        numeric_delta(first.aggregate.minimum_cache_lines, second.aggregate.minimum_cache_lines);
    result.complete_elements_per_cache_line_delta =
        numeric_delta(first.aggregate.complete_elements_per_cache_line,
                      second.aggregate.complete_elements_per_cache_line);
    result.cache_line_straddling_delta =
        numeric_delta(first.aggregate.cache_line_straddling_elements,
                      second.aggregate.cache_line_straddling_elements);
    result.page_size_delta = numeric_delta(first.aggregate.page_bytes, second.aggregate.page_bytes);
    result.minimum_page_delta =
        numeric_delta(first.aggregate.minimum_pages, second.aggregate.minimum_pages);
    result.complete_elements_per_page_delta = numeric_delta(
        first.aggregate.complete_elements_per_page, second.aggregate.complete_elements_per_page);
    result.page_straddling_delta = numeric_delta(first.aggregate.page_straddling_elements,
                                                 second.aggregate.page_straddling_elements);
    return result;
}

auto Analyzer::analyze_union(lispb::schema::TypeGraph const& types,
                             lispb::schema::TypeId const type,
                             AbiProfile const& abi,
                             std::uint64_t const element_count) -> UnionAnalysis {
    auto result{PhysicalFactsResolver{types, abi}.analyze_union(type)};
    result.aggregate.element_count = element_count;

    auto scale = [&](std::optional<std::uint64_t> const value,
                     std::string const& description) -> std::optional<std::uint64_t> {
        if (!value.has_value()) {
            return std::nullopt;
        }
        auto const scaled{checked_multiply(*value, element_count)};
        if (!scaled.has_value()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Union aggregate " + description + " overflows uint64."});
        }
        return scaled;
    };
    result.aggregate.total_storage_bytes = scale(result.size_bytes, "storage byte count");
    result.aggregate.total_tail_padding_bytes =
        scale(result.tail_padding_bytes, "tail-padding byte count");
    for (auto& alternative : result.alternatives) {
        alternative.total_slack_bytes =
            scale(alternative.slack_bytes, "alternative '" + alternative.name + "' slack");
    }
    result.aggregate.cache_capacity =
        cache_capacity_analysis(result.aggregate.total_storage_bytes, abi.memory_facts());

    if (result.size_bytes.has_value() && *result.size_bytes != 0) {
        auto const& memory{abi.memory_facts()};
        result.aggregate.cache_line_bytes = memory.cache_line_bytes;
        if (!memory.cache_line_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Cache-line size is unknown for ABI profile '" + abi.name() + "'."});
        } else if (*memory.cache_line_bytes == 0) {
            result.aggregate.cache_line_bytes.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "ABI profile cache-line size must be non-zero."});
        } else {
            if (result.aggregate.total_storage_bytes.has_value()) {
                result.aggregate.minimum_cache_lines = minimum_regions(
                    *result.aggregate.total_storage_bytes, *memory.cache_line_bytes);
            }
            result.aggregate.complete_elements_per_cache_line =
                *memory.cache_line_bytes / *result.size_bytes;
            result.aggregate.cache_line_straddling_elements =
                straddling_elements(*result.size_bytes,
                                    element_count,
                                    *memory.cache_line_bytes,
                                    result.aggregate.total_storage_bytes);
        }

        result.aggregate.page_bytes = memory.page_bytes;
        if (!memory.page_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Page size is unknown for ABI profile '" + abi.name() + "'."});
        } else if (*memory.page_bytes == 0) {
            result.aggregate.page_bytes.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "ABI profile page size must be non-zero."});
        } else {
            if (result.aggregate.total_storage_bytes.has_value()) {
                result.aggregate.minimum_pages =
                    minimum_regions(*result.aggregate.total_storage_bytes, *memory.page_bytes);
            }
            result.aggregate.complete_elements_per_page = *memory.page_bytes / *result.size_bytes;
            result.aggregate.page_straddling_elements =
                straddling_elements(*result.size_bytes,
                                    element_count,
                                    *memory.page_bytes,
                                    result.aggregate.total_storage_bytes);
        }
    }
    return result;
}

auto Analyzer::compare_union_targets(UnionAnalysis const& first, UnionAnalysis const& second)
    -> UnionTargetComparison {
    UnionTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target raw union: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target raw union: " + diagnostic.message});
    }
    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Compared target analyses describe different raw unions."});
        return result;
    }
    if (first.aggregate.element_count != second.aggregate.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target raw-union analyses use different element counts."});
        return result;
    }

    std::set<std::string, std::less<>> first_names;
    for (auto const& alternative : first.alternatives) {
        first_names.insert(alternative.name);
    }
    std::set<std::string, std::less<>> second_names;
    for (auto const& alternative : second.alternatives) {
        second_names.insert(alternative.name);
    }
    if (first_names.size() != first.alternatives.size() ||
        second_names.size() != second.alternatives.size() || first_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target raw-union analyses do not contain the same unique alternatives."});
        return result;
    }

    for (auto const& first_alternative : first.alternatives) {
        auto const second_alternative{std::ranges::find(
            second.alternatives, first_alternative.name, &UnionAlternativeAnalysis::name)};
        if (second_alternative == second.alternatives.end() ||
            first_alternative.semantic_type != second_alternative->semantic_type ||
            first_alternative.element_count != second_alternative->element_count) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target raw-union alternative '" + first_alternative.name +
                     "' has incompatible semantic identity or element count."});
            return result;
        }
    }

    result.alternatives.reserve(first.alternatives.size());
    for (auto const& first_alternative : first.alternatives) {
        auto const second_alternative{std::ranges::find(
            second.alternatives, first_alternative.name, &UnionAlternativeAnalysis::name)};
        auto const first_size{first_alternative.element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_size{second_alternative->element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_alignment{first_alternative.element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_alignment{second_alternative->element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        result.alternatives.push_back(
            {.name = first_alternative.name,
             .first = first_alternative,
             .second = *second_alternative,
             .element_size_delta = numeric_delta(first_size, second_size),
             .element_alignment_delta = numeric_delta(first_alignment, second_alignment),
             .extent_delta =
                 numeric_delta(first_alternative.extent_bytes, second_alternative->extent_bytes),
             .slack_delta =
                 numeric_delta(first_alternative.slack_bytes, second_alternative->slack_bytes),
             .total_slack_delta = numeric_delta(first_alternative.total_slack_bytes,
                                                second_alternative->total_slack_bytes)});
    }

    result.largest_alternative_delta =
        numeric_delta(first.largest_alternative_bytes, second.largest_alternative_bytes);
    result.tail_padding_delta = numeric_delta(first.tail_padding_bytes, second.tail_padding_bytes);
    result.size_delta = numeric_delta(first.size_bytes, second.size_bytes);
    result.alignment_delta = numeric_delta(first.alignment_bytes, second.alignment_bytes);
    result.total_storage_delta =
        numeric_delta(first.aggregate.total_storage_bytes, second.aggregate.total_storage_bytes);
    result.total_tail_padding_delta = numeric_delta(first.aggregate.total_tail_padding_bytes,
                                                    second.aggregate.total_tail_padding_bytes);
    result.cache_line_size_delta =
        numeric_delta(first.aggregate.cache_line_bytes, second.aggregate.cache_line_bytes);
    result.minimum_cache_line_delta =
        numeric_delta(first.aggregate.minimum_cache_lines, second.aggregate.minimum_cache_lines);
    result.complete_elements_per_cache_line_delta =
        numeric_delta(first.aggregate.complete_elements_per_cache_line,
                      second.aggregate.complete_elements_per_cache_line);
    result.cache_line_straddling_delta =
        numeric_delta(first.aggregate.cache_line_straddling_elements,
                      second.aggregate.cache_line_straddling_elements);
    result.page_size_delta = numeric_delta(first.aggregate.page_bytes, second.aggregate.page_bytes);
    result.minimum_page_delta =
        numeric_delta(first.aggregate.minimum_pages, second.aggregate.minimum_pages);
    result.complete_elements_per_page_delta = numeric_delta(
        first.aggregate.complete_elements_per_page, second.aggregate.complete_elements_per_page);
    result.page_straddling_delta = numeric_delta(first.aggregate.page_straddling_elements,
                                                 second.aggregate.page_straddling_elements);
    return result;
}

auto Analyzer::analyze_tagged_union(lispb::schema::TypeGraph const& types,
                                    lispb::schema::TypeId const type,
                                    AbiProfile const& abi,
                                    std::uint64_t const element_count) -> TaggedUnionAnalysis {
    auto result{PhysicalFactsResolver{types, abi}.analyze_tagged_union(type)};
    result.aggregate.element_count = element_count;

    auto scale = [&](std::optional<std::uint64_t> const value,
                     std::string const& description) -> std::optional<std::uint64_t> {
        if (!value.has_value()) {
            return std::nullopt;
        }
        auto const scaled{checked_multiply(*value, element_count)};
        if (!scaled.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Tagged-union aggregate " + description + " overflows uint64."});
        }
        return scaled;
    };
    result.aggregate.total_storage_bytes = scale(result.size_bytes, "storage byte count");
    result.aggregate.total_discriminant_bytes = scale(
        result.discriminant_facts.has_value() ? std::optional{result.discriminant_facts->size_bytes}
                                              : std::nullopt,
        "discriminant byte count");
    result.aggregate.total_payload_bytes = scale(result.payload_size_bytes, "payload byte count");
    result.aggregate.total_internal_padding_bytes =
        scale(result.internal_padding_bytes, "internal-padding byte count");
    result.aggregate.total_tail_padding_bytes =
        scale(result.tail_padding_bytes, "tail-padding byte count");
    if (result.aggregate.total_internal_padding_bytes.has_value() &&
        result.aggregate.total_tail_padding_bytes.has_value()) {
        result.aggregate.total_padding_bytes =
            checked_add(*result.aggregate.total_internal_padding_bytes,
                        *result.aggregate.total_tail_padding_bytes);
        if (!result.aggregate.total_padding_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Tagged-union aggregate padding byte count overflows uint64."});
        }
    }
    for (auto& alternative : result.alternatives) {
        alternative.total_payload_slack_bytes =
            scale(alternative.payload_slack_bytes,
                  "alternative '" + alternative.name + "' payload slack");
    }
    result.aggregate.cache_capacity =
        cache_capacity_analysis(result.aggregate.total_storage_bytes, abi.memory_facts());

    if (result.size_bytes.has_value() && *result.size_bytes != 0) {
        auto const& memory{abi.memory_facts()};
        result.aggregate.cache_line_bytes = memory.cache_line_bytes;
        if (!memory.cache_line_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Cache-line size is unknown for ABI profile '" + abi.name() + "'."});
        } else if (*memory.cache_line_bytes == 0) {
            result.aggregate.cache_line_bytes.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "ABI profile cache-line size must be non-zero."});
        } else {
            if (result.aggregate.total_storage_bytes.has_value()) {
                result.aggregate.minimum_cache_lines = minimum_regions(
                    *result.aggregate.total_storage_bytes, *memory.cache_line_bytes);
            }
            result.aggregate.complete_elements_per_cache_line =
                *memory.cache_line_bytes / *result.size_bytes;
            result.aggregate.cache_line_straddling_elements =
                straddling_elements(*result.size_bytes,
                                    element_count,
                                    *memory.cache_line_bytes,
                                    result.aggregate.total_storage_bytes);
        }

        result.aggregate.page_bytes = memory.page_bytes;
        if (!memory.page_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Page size is unknown for ABI profile '" + abi.name() + "'."});
        } else if (*memory.page_bytes == 0) {
            result.aggregate.page_bytes.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "ABI profile page size must be non-zero."});
        } else {
            if (result.aggregate.total_storage_bytes.has_value()) {
                result.aggregate.minimum_pages =
                    minimum_regions(*result.aggregate.total_storage_bytes, *memory.page_bytes);
            }
            result.aggregate.complete_elements_per_page = *memory.page_bytes / *result.size_bytes;
            result.aggregate.page_straddling_elements =
                straddling_elements(*result.size_bytes,
                                    element_count,
                                    *memory.page_bytes,
                                    result.aggregate.total_storage_bytes);
        }
    }
    return result;
}

auto Analyzer::compare_tagged_union_targets(TaggedUnionAnalysis const& first,
                                            TaggedUnionAnalysis const& second)
    -> TaggedUnionTargetComparison {
    TaggedUnionTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target tagged union: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target tagged union: " + diagnostic.message});
    }
    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target analyses describe different tagged unions."});
        return result;
    }
    if (first.aggregate.element_count != second.aggregate.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target tagged-union analyses use different element counts."});
        return result;
    }
    if (first.discriminant_type != second.discriminant_type ||
        first.mapped_live_tags != second.mapped_live_tags ||
        first.unmapped_live_tags != second.unmapped_live_tags ||
        first.sentinel_tags != second.sentinel_tags ||
        first.count_sentinel_tag != second.count_sentinel_tag) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target tagged-union analyses have incompatible discriminant or tag "
             "coverage semantics."});
        return result;
    }

    std::set<std::string, std::less<>> first_names;
    std::set<std::string, std::less<>> first_tags;
    for (auto const& alternative : first.alternatives) {
        first_names.insert(alternative.name);
        first_tags.insert(alternative.tag);
    }
    std::set<std::string, std::less<>> second_names;
    std::set<std::string, std::less<>> second_tags;
    for (auto const& alternative : second.alternatives) {
        second_names.insert(alternative.name);
        second_tags.insert(alternative.tag);
    }
    if (first_names.size() != first.alternatives.size() ||
        first_tags.size() != first.alternatives.size() ||
        second_names.size() != second.alternatives.size() ||
        second_tags.size() != second.alternatives.size() || first_names != second_names ||
        first_tags != second_tags) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target tagged-union analyses do not contain the same unique alternatives "
             "and tags."});
        return result;
    }
    for (auto const& first_alternative : first.alternatives) {
        auto const second_alternative{std::ranges::find(
            second.alternatives, first_alternative.name, &TaggedUnionAlternativeAnalysis::name)};
        if (second_alternative == second.alternatives.end() ||
            first_alternative.tag != second_alternative->tag ||
            first_alternative.semantic_type != second_alternative->semantic_type ||
            first_alternative.element_count != second_alternative->element_count) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target tagged-union alternative '" + first_alternative.name +
                     "' has incompatible tag, semantic identity, or element count."});
            return result;
        }
    }

    result.alternatives.reserve(first.alternatives.size());
    for (auto const& first_alternative : first.alternatives) {
        auto const second_alternative{std::ranges::find(
            second.alternatives, first_alternative.name, &TaggedUnionAlternativeAnalysis::name)};
        auto const first_size{first_alternative.element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const second_size{second_alternative->element_facts.transform(
            [](TypeFacts const& facts) { return facts.size_bytes; })};
        auto const first_alignment{first_alternative.element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        auto const second_alignment{second_alternative->element_facts.transform(
            [](TypeFacts const& facts) { return facts.alignment_bytes; })};
        result.alternatives.push_back(
            {.name = first_alternative.name,
             .first = first_alternative,
             .second = *second_alternative,
             .element_size_delta = numeric_delta(first_size, second_size),
             .element_alignment_delta = numeric_delta(first_alignment, second_alignment),
             .extent_delta =
                 numeric_delta(first_alternative.extent_bytes, second_alternative->extent_bytes),
             .payload_slack_delta = numeric_delta(first_alternative.payload_slack_bytes,
                                                  second_alternative->payload_slack_bytes),
             .total_payload_slack_delta =
                 numeric_delta(first_alternative.total_payload_slack_bytes,
                               second_alternative->total_payload_slack_bytes)});
    }

    auto const first_discriminant_size{first.discriminant_facts.transform(
        [](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const second_discriminant_size{second.discriminant_facts.transform(
        [](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const first_discriminant_alignment{first.discriminant_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    auto const second_discriminant_alignment{second.discriminant_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    result.discriminant_size_delta =
        numeric_delta(first_discriminant_size, second_discriminant_size);
    result.discriminant_alignment_delta =
        numeric_delta(first_discriminant_alignment, second_discriminant_alignment);
    result.largest_alternative_delta =
        numeric_delta(first.largest_alternative_bytes, second.largest_alternative_bytes);
    result.payload_size_delta = numeric_delta(first.payload_size_bytes, second.payload_size_bytes);
    result.payload_alignment_delta =
        numeric_delta(first.payload_alignment_bytes, second.payload_alignment_bytes);
    result.payload_offset_delta =
        numeric_delta(first.payload_offset_bytes, second.payload_offset_bytes);
    result.internal_padding_delta =
        numeric_delta(first.internal_padding_bytes, second.internal_padding_bytes);
    result.tail_padding_delta = numeric_delta(first.tail_padding_bytes, second.tail_padding_bytes);
    result.size_delta = numeric_delta(first.size_bytes, second.size_bytes);
    result.alignment_delta = numeric_delta(first.alignment_bytes, second.alignment_bytes);
    result.total_storage_delta =
        numeric_delta(first.aggregate.total_storage_bytes, second.aggregate.total_storage_bytes);
    result.total_discriminant_delta = numeric_delta(first.aggregate.total_discriminant_bytes,
                                                    second.aggregate.total_discriminant_bytes);
    result.total_payload_delta =
        numeric_delta(first.aggregate.total_payload_bytes, second.aggregate.total_payload_bytes);
    result.total_internal_padding_delta =
        numeric_delta(first.aggregate.total_internal_padding_bytes,
                      second.aggregate.total_internal_padding_bytes);
    result.total_tail_padding_delta = numeric_delta(first.aggregate.total_tail_padding_bytes,
                                                    second.aggregate.total_tail_padding_bytes);
    result.total_padding_delta =
        numeric_delta(first.aggregate.total_padding_bytes, second.aggregate.total_padding_bytes);
    result.cache_line_size_delta =
        numeric_delta(first.aggregate.cache_line_bytes, second.aggregate.cache_line_bytes);
    result.minimum_cache_line_delta =
        numeric_delta(first.aggregate.minimum_cache_lines, second.aggregate.minimum_cache_lines);
    result.complete_elements_per_cache_line_delta =
        numeric_delta(first.aggregate.complete_elements_per_cache_line,
                      second.aggregate.complete_elements_per_cache_line);
    result.cache_line_straddling_delta =
        numeric_delta(first.aggregate.cache_line_straddling_elements,
                      second.aggregate.cache_line_straddling_elements);
    result.page_size_delta = numeric_delta(first.aggregate.page_bytes, second.aggregate.page_bytes);
    result.minimum_page_delta =
        numeric_delta(first.aggregate.minimum_pages, second.aggregate.minimum_pages);
    result.complete_elements_per_page_delta = numeric_delta(
        first.aggregate.complete_elements_per_page, second.aggregate.complete_elements_per_page);
    result.page_straddling_delta = numeric_delta(first.aggregate.page_straddling_elements,
                                                 second.aggregate.page_straddling_elements);
    return result;
}

auto Analyzer::analyze_union_distribution(UnionAnalysis const& analysis,
                                          std::span<UnionDistributionEntry const> const entries,
                                          std::uint64_t const selected_element_count)
    -> UnionDistributionAnalysis {
    UnionDistributionAnalysis result{.type = analysis.type,
                                     .valid_entry_count = 0,
                                     .total_weight = 0,
                                     .total_extent_bytes = 0,
                                     .total_slack_bytes = 0,
                                     .expected_extent_bytes_per_value = std::nullopt,
                                     .expected_slack_bytes_per_value = std::nullopt,
                                     .selected_element_count = selected_element_count,
                                     .expected_selected_extent_bytes = std::nullopt,
                                     .expected_selected_slack_bytes = std::nullopt,
                                     .entries = {},
                                     .diagnostics = {}};
    std::set<std::string, std::less<>> seen_alternatives;
    auto exact_weight_overflow_reported{false};
    auto exact_extent_overflow_reported{false};
    auto exact_slack_overflow_reported{false};
    auto numerical_weight{0.0L};
    auto numerical_extent{0.0L};
    auto numerical_slack{0.0L};
    auto numerical_complete{true};

    for (auto const& entry : entries) {
        if (!seen_alternatives.insert(entry.alternative_name).second) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Raw-union distribution contains duplicate alternative '" +
                     entry.alternative_name + "'."});
            continue;
        }
        auto const alternative{std::ranges::find(
            analysis.alternatives, entry.alternative_name, &UnionAlternativeAnalysis::name)};
        if (alternative == analysis.alternatives.end()) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Raw-union distribution contains unknown alternative '" +
                                              entry.alternative_name + "'."});
            continue;
        }

        ++result.valid_entry_count;
        auto const weighted_extent{alternative->extent_bytes.has_value()
                                       ? checked_multiply(entry.weight, *alternative->extent_bytes)
                                       : std::nullopt};
        auto const weighted_slack{alternative->slack_bytes.has_value()
                                      ? checked_multiply(entry.weight, *alternative->slack_bytes)
                                      : std::nullopt};
        result.entries.push_back({.alternative_name = entry.alternative_name,
                                  .weight = entry.weight,
                                  .extent_bytes = alternative->extent_bytes,
                                  .slack_bytes = alternative->slack_bytes,
                                  .weighted_extent_bytes = weighted_extent,
                                  .weighted_slack_bytes = weighted_slack});
        numerical_weight += static_cast<long double>(entry.weight);
        if (alternative->extent_bytes.has_value() && alternative->slack_bytes.has_value()) {
            numerical_extent += static_cast<long double>(entry.weight) *
                                static_cast<long double>(*alternative->extent_bytes);
            numerical_slack += static_cast<long double>(entry.weight) *
                               static_cast<long double>(*alternative->slack_bytes);
        } else if (entry.weight != 0) {
            numerical_complete = false;
        }

        if (result.total_weight.has_value()) {
            result.total_weight = checked_add(*result.total_weight, entry.weight);
            if (!result.total_weight.has_value() && !exact_weight_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Raw-union distribution weight total overflows uint64."});
                exact_weight_overflow_reported = true;
            }
        }
        if (result.total_extent_bytes.has_value()) {
            result.total_extent_bytes =
                weighted_extent.has_value()
                    ? checked_add(*result.total_extent_bytes, *weighted_extent)
                    : std::nullopt;
            if (!result.total_extent_bytes.has_value() && !exact_extent_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Raw-union distribution extent total overflows or is Unknown."});
                exact_extent_overflow_reported = true;
            }
        }
        if (result.total_slack_bytes.has_value()) {
            result.total_slack_bytes = weighted_slack.has_value()
                                         ? checked_add(*result.total_slack_bytes, *weighted_slack)
                                         : std::nullopt;
            if (!result.total_slack_bytes.has_value() && !exact_slack_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Raw-union distribution slack total overflows or is Unknown."});
                exact_slack_overflow_reported = true;
            }
        }
    }

    if (numerical_weight > 0.0L && numerical_complete) {
        result.expected_extent_bytes_per_value = numerical_extent / numerical_weight;
        result.expected_slack_bytes_per_value = numerical_slack / numerical_weight;
        result.expected_selected_extent_bytes = *result.expected_extent_bytes_per_value *
                                                static_cast<long double>(selected_element_count);
        result.expected_selected_slack_bytes = *result.expected_slack_bytes_per_value *
                                               static_cast<long double>(selected_element_count);
    } else {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Expected raw-union alternative usage is Unknown because the distribution has no "
             "positive valid weight with complete target facts."});
    }
    return result;
}

auto Analyzer::compare_union_distributions(UnionDistributionAnalysis const& first,
                                           UnionDistributionAnalysis const& second)
    -> UnionDistributionComparison {
    UnionDistributionComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target raw-union distribution: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target raw-union distribution: " + diagnostic.message});
    }
    if (first.type != second.type ||
        first.selected_element_count != second.selected_element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared raw-union distributions use different types or selected counts."});
        return result;
    }
    if (first.valid_entry_count != second.valid_entry_count ||
        first.entries.size() != second.entries.size()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared raw-union distributions do not contain the same valid entries."});
        return result;
    }

    std::set<std::string, std::less<>> first_names;
    for (auto const& entry : first.entries) {
        first_names.insert(entry.alternative_name);
    }
    std::set<std::string, std::less<>> second_names;
    for (auto const& entry : second.entries) {
        second_names.insert(entry.alternative_name);
    }
    if (first_names.size() != first.entries.size() ||
        second_names.size() != second.entries.size() || first_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared raw-union distributions do not contain the same unique alternatives."});
        return result;
    }
    for (auto const& first_entry : first.entries) {
        auto const second_entry{
            std::ranges::find(second.entries,
                              first_entry.alternative_name,
                              &UnionDistributionEntryAnalysis::alternative_name)};
        if (second_entry == second.entries.end() || first_entry.weight != second_entry->weight) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Compared raw-union distribution entry '" +
                                              first_entry.alternative_name +
                                              "' has an incompatible weight."});
            return result;
        }
    }

    result.entries.reserve(first.entries.size());
    for (auto const& first_entry : first.entries) {
        auto const second_entry{
            std::ranges::find(second.entries,
                              first_entry.alternative_name,
                              &UnionDistributionEntryAnalysis::alternative_name)};
        result.entries.push_back(
            {.alternative_name = first_entry.alternative_name,
             .first = first_entry,
             .second = *second_entry,
             .extent_delta = numeric_delta(first_entry.extent_bytes, second_entry->extent_bytes),
             .slack_delta = numeric_delta(first_entry.slack_bytes, second_entry->slack_bytes),
             .weighted_extent_delta = numeric_delta(first_entry.weighted_extent_bytes,
                                                    second_entry->weighted_extent_bytes),
             .weighted_slack_delta = numeric_delta(first_entry.weighted_slack_bytes,
                                                   second_entry->weighted_slack_bytes)});
    }
    result.total_weight_delta = numeric_delta(first.total_weight, second.total_weight);
    result.total_extent_delta = numeric_delta(first.total_extent_bytes, second.total_extent_bytes);
    result.total_slack_delta = numeric_delta(first.total_slack_bytes, second.total_slack_bytes);
    auto decimal_delta = [](std::optional<long double> const first_value,
                            std::optional<long double> const second_value) {
        return first_value.has_value() && second_value.has_value()
                 ? std::optional{*second_value - *first_value}
                 : std::nullopt;
    };
    result.expected_extent_per_value_delta = decimal_delta(first.expected_extent_bytes_per_value,
                                                           second.expected_extent_bytes_per_value);
    result.expected_slack_per_value_delta =
        decimal_delta(first.expected_slack_bytes_per_value, second.expected_slack_bytes_per_value);
    result.expected_selected_extent_delta =
        decimal_delta(first.expected_selected_extent_bytes, second.expected_selected_extent_bytes);
    result.expected_selected_slack_delta =
        decimal_delta(first.expected_selected_slack_bytes, second.expected_selected_slack_bytes);
    return result;
}

auto Analyzer::analyze_tagged_union_distribution(
    TaggedUnionAnalysis const& tagged_union,
    std::span<TaggedUnionDistributionEntry const> const entries,
    std::uint64_t const selected_element_count) -> TaggedUnionDistributionAnalysis {
    TaggedUnionDistributionAnalysis result{.type = tagged_union.type,
                                           .valid_entry_count = 0,
                                           .total_weight = 0,
                                           .total_payload_extent_bytes = 0,
                                           .total_payload_slack_bytes = 0,
                                           .expected_payload_extent_bytes_per_value = std::nullopt,
                                           .expected_payload_slack_bytes_per_value = std::nullopt,
                                           .selected_element_count = selected_element_count,
                                           .expected_selected_payload_extent_bytes = std::nullopt,
                                           .expected_selected_payload_slack_bytes = std::nullopt,
                                           .entries = {},
                                           .diagnostics = {}};
    std::set<std::string, std::less<>> seen_tags;
    auto exact_weight_overflow_reported{false};
    auto exact_extent_overflow_reported{false};
    auto exact_slack_overflow_reported{false};
    auto numerical_weight{0.0L};
    auto numerical_extent{0.0L};
    auto numerical_slack{0.0L};
    auto numerical_complete{true};

    for (auto const& entry : entries) {
        if (!seen_tags.insert(entry.tag).second) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Tagged-union distribution contains duplicate tag '" + entry.tag + "'."});
            continue;
        }
        auto const alternative{std::ranges::find(
            tagged_union.alternatives, entry.tag, &TaggedUnionAlternativeAnalysis::tag)};
        if (alternative == tagged_union.alternatives.end()) {
            auto role{std::string{"unknown"}};
            if (std::ranges::find(tagged_union.unmapped_live_tags, entry.tag) !=
                tagged_union.unmapped_live_tags.end()) {
                role = "unmapped live";
            } else if (std::ranges::find(tagged_union.sentinel_tags, entry.tag) !=
                       tagged_union.sentinel_tags.end()) {
                role = "sentinel";
            } else if (tagged_union.count_sentinel_tag == entry.tag) {
                role = "count sentinel";
            }
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Tagged-union distribution tag '" + entry.tag + "' is " +
                                              role + " and has no payload alternative."});
            continue;
        }

        ++result.valid_entry_count;
        auto const weighted_extent{alternative->extent_bytes.has_value()
                                       ? checked_multiply(entry.weight, *alternative->extent_bytes)
                                       : std::nullopt};
        auto const weighted_slack{
            alternative->payload_slack_bytes.has_value()
                ? checked_multiply(entry.weight, *alternative->payload_slack_bytes)
                : std::nullopt};
        result.entries.push_back({.tag = entry.tag,
                                  .alternative_name = alternative->name,
                                  .weight = entry.weight,
                                  .payload_extent_bytes = alternative->extent_bytes,
                                  .payload_slack_bytes = alternative->payload_slack_bytes,
                                  .weighted_payload_extent_bytes = weighted_extent,
                                  .weighted_payload_slack_bytes = weighted_slack});
        numerical_weight += static_cast<long double>(entry.weight);
        if (alternative->extent_bytes.has_value() && alternative->payload_slack_bytes.has_value()) {
            numerical_extent += static_cast<long double>(entry.weight) *
                                static_cast<long double>(*alternative->extent_bytes);
            numerical_slack += static_cast<long double>(entry.weight) *
                               static_cast<long double>(*alternative->payload_slack_bytes);
        } else if (entry.weight != 0) {
            numerical_complete = false;
        }

        if (result.total_weight.has_value()) {
            result.total_weight = checked_add(*result.total_weight, entry.weight);
            if (!result.total_weight.has_value() && !exact_weight_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Tagged-union distribution weight total overflows uint64."});
                exact_weight_overflow_reported = true;
            }
        }
        if (result.total_payload_extent_bytes.has_value()) {
            result.total_payload_extent_bytes =
                weighted_extent.has_value()
                    ? checked_add(*result.total_payload_extent_bytes, *weighted_extent)
                    : std::nullopt;
            if (!result.total_payload_extent_bytes.has_value() && !exact_extent_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Tagged-union distribution payload-extent total overflows or is Unknown."});
                exact_extent_overflow_reported = true;
            }
        }
        if (result.total_payload_slack_bytes.has_value()) {
            result.total_payload_slack_bytes =
                weighted_slack.has_value()
                    ? checked_add(*result.total_payload_slack_bytes, *weighted_slack)
                    : std::nullopt;
            if (!result.total_payload_slack_bytes.has_value() && !exact_slack_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Tagged-union distribution payload-slack total overflows or is Unknown."});
                exact_slack_overflow_reported = true;
            }
        }
    }

    if (numerical_weight > 0.0L && numerical_complete) {
        result.expected_payload_extent_bytes_per_value = numerical_extent / numerical_weight;
        result.expected_payload_slack_bytes_per_value = numerical_slack / numerical_weight;
        result.expected_selected_payload_extent_bytes =
            *result.expected_payload_extent_bytes_per_value *
            static_cast<long double>(selected_element_count);
        result.expected_selected_payload_slack_bytes =
            *result.expected_payload_slack_bytes_per_value *
            static_cast<long double>(selected_element_count);
    } else {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Expected tagged-union payload usage is Unknown because the distribution has no "
             "positive valid weight with complete target facts."});
    }
    return result;
}

auto Analyzer::compare_tagged_union_distributions(TaggedUnionDistributionAnalysis const& first,
                                                  TaggedUnionDistributionAnalysis const& second)
    -> TaggedUnionDistributionComparison {
    TaggedUnionDistributionComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target tagged-union distribution: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity,
             "Second target tagged-union distribution: " + diagnostic.message});
    }
    if (first.type != second.type ||
        first.selected_element_count != second.selected_element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared tagged-union distributions use different types or selected counts."});
        return result;
    }
    if (first.valid_entry_count != second.valid_entry_count ||
        first.entries.size() != second.entries.size()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared tagged-union distributions do not contain the same valid entries."});
        return result;
    }

    std::set<std::string, std::less<>> first_tags;
    for (auto const& entry : first.entries) {
        first_tags.insert(entry.tag);
    }
    std::set<std::string, std::less<>> second_tags;
    for (auto const& entry : second.entries) {
        second_tags.insert(entry.tag);
    }
    if (first_tags.size() != first.entries.size() || second_tags.size() != second.entries.size() ||
        first_tags != second_tags) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared tagged-union distributions do not contain the same unique tags."});
        return result;
    }
    for (auto const& first_entry : first.entries) {
        auto const second_entry{std::ranges::find(
            second.entries, first_entry.tag, &TaggedUnionDistributionEntryAnalysis::tag)};
        if (second_entry == second.entries.end() ||
            first_entry.alternative_name != second_entry->alternative_name ||
            first_entry.weight != second_entry->weight) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared tagged-union distribution entry '" + first_entry.tag +
                     "' has incompatible alternative identity or weight."});
            return result;
        }
    }

    result.entries.reserve(first.entries.size());
    for (auto const& first_entry : first.entries) {
        auto const second_entry{std::ranges::find(
            second.entries, first_entry.tag, &TaggedUnionDistributionEntryAnalysis::tag)};
        result.entries.push_back(
            {.tag = first_entry.tag,
             .first = first_entry,
             .second = *second_entry,
             .payload_extent_delta = numeric_delta(first_entry.payload_extent_bytes,
                                                   second_entry->payload_extent_bytes),
             .payload_slack_delta =
                 numeric_delta(first_entry.payload_slack_bytes, second_entry->payload_slack_bytes),
             .weighted_payload_extent_delta =
                 numeric_delta(first_entry.weighted_payload_extent_bytes,
                               second_entry->weighted_payload_extent_bytes),
             .weighted_payload_slack_delta =
                 numeric_delta(first_entry.weighted_payload_slack_bytes,
                               second_entry->weighted_payload_slack_bytes)});
    }
    result.total_weight_delta = numeric_delta(first.total_weight, second.total_weight);
    result.total_payload_extent_delta =
        numeric_delta(first.total_payload_extent_bytes, second.total_payload_extent_bytes);
    result.total_payload_slack_delta =
        numeric_delta(first.total_payload_slack_bytes, second.total_payload_slack_bytes);
    auto decimal_delta = [](std::optional<long double> const first_value,
                            std::optional<long double> const second_value) {
        return first_value.has_value() && second_value.has_value()
                 ? std::optional{*second_value - *first_value}
                 : std::nullopt;
    };
    result.expected_payload_extent_per_value_delta =
        decimal_delta(first.expected_payload_extent_bytes_per_value,
                      second.expected_payload_extent_bytes_per_value);
    result.expected_payload_slack_per_value_delta =
        decimal_delta(first.expected_payload_slack_bytes_per_value,
                      second.expected_payload_slack_bytes_per_value);
    result.expected_selected_payload_extent_delta =
        decimal_delta(first.expected_selected_payload_extent_bytes,
                      second.expected_selected_payload_extent_bytes);
    result.expected_selected_payload_slack_delta = decimal_delta(
        first.expected_selected_payload_slack_bytes, second.expected_selected_payload_slack_bytes);
    return result;
}

} // namespace ioj::layout
