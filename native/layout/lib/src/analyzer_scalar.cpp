#include "checked_arithmetic.hpp"
#include "enum_backing.hpp"
#include "integer_code_domain.hpp"
#include "memory_regions.hpp"
#include "relationship_extent.hpp"
#include "varint_encoding.hpp"
#include <ioj/layout/analyzer.hpp>
#include <ioj/layout/physical_facts.hpp>
#include <ioj/layout/string_set.hpp>

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <lispb/project_loader.h>
#include <lispb/schema/enum_domain_issue_severity.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_domain.h>
#include <lispb/schema/integer_varint_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/optional_presence_bit_types.h>
#include <lispb/schema/optional_sentinel_types.h>
#include <lispb/schema/type_definitions.h>
#include <lispb/strings.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ioj::layout {
using namespace analysis_detail;

auto Analyzer::analyze_enum(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId const type,
                            AbiProfile const& abi,
                            std::uint64_t const element_count) -> EnumDomainAnalysis {
    auto const& enumeration{types.definition(type)};
    auto const signedness{
        types.definitions().enums.signedness[types.definitions().enums.index(enumeration)]};
    auto const declared_signedness{
        signedness == lispb::schema::EnumSignedness::unspecified
            ? std::nullopt
            : std::optional{signedness == lispb::schema::EnumSignedness::signed_}};
    auto const backing_type{
        lispb::schema::is_valid(
            types.definitions().enums.underlying_type[types.definitions().enums.index(enumeration)])
            ? physical_type_spelling(
                  types,
                  types.definitions()
                      .enums.underlying_type[types.definitions().enums.index(enumeration)])
                  .value_or(std::string{types.cpp_types().spelling(
                      types.occurrences().cpp_type[types.occurrences().index(
                          types.definitions().enums.underlying_type[types.definitions().enums.index(
                              enumeration)])])})
            : derived_enum_backing_type(types, enumeration).value_or("Unknown")};
    auto physical{PhysicalFactsResolver{types, abi}.resolve(type)};
    EnumDomainAnalysis result{
        .type = type,
        .backing_type = backing_type,
        .backing_facts = physical.facts,
        .backing_bits = std::nullopt,
        .live_value_count = 0,
        .reserved_value_count = 0,
        .minimum_value = std::nullopt,
        .maximum_value = std::nullopt,
        .signed_domain = std::nullopt,
        .declared_signedness = declared_signedness,
        .minimum_required_bits = std::nullopt,
        .declared_bit_width =
            types.definitions().enums.bit_width[types.definitions().enums.index(enumeration)],
        .effective_bit_width = std::nullopt,
        .semantic_width_can_represent_domain = std::nullopt,
        .unused_semantic_codes = std::nullopt,
        .backing_can_represent_domain = std::nullopt,
        .unused_backing_codes = std::nullopt,
        .enumerators = {},
        .diagnostics = std::move(physical.diagnostics),
        .aggregate = {.element_count = element_count,
                      .total_storage_bytes = std::nullopt,
                      .cache_line_bytes = std::nullopt,
                      .minimum_cache_lines = std::nullopt,
                      .complete_elements_per_cache_line = std::nullopt,
                      .cache_line_straddling_elements = std::nullopt,
                      .page_bytes = std::nullopt,
                      .minimum_pages = std::nullopt,
                      .complete_elements_per_page = std::nullopt,
                      .page_straddling_elements = std::nullopt,
                      .cache_capacity = {}}};
    if (backing_type == "Unknown") {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Enum C++ backing storage cannot be derived because semantic signedness or width is "
             "unknown."});
    } else if (!result.backing_facts.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Unknown physical facts for enum backing type '" + backing_type + "'."});
    } else {
        result.backing_bits = checked_multiply(result.backing_facts->size_bytes, 8);
        if (!result.backing_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Enum backing bit count overflows uint64."});
        } else if (*result.backing_bits == 0) {
            result.backing_bits.reset();
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Enum backing type has a zero-byte physical size."});
        }
    }

    auto const& domains{types.enum_domains()};
    auto const domain{
        types.definitions().enums.domain[types.definitions().enums.index(enumeration)]};
    auto const domain_row{domains.index(domain)};
    result.live_value_count = domains.live_value_count[domain_row];
    result.reserved_value_count = domains.reserved_value_count[domain_row];
    result.minimum_value = domains.minimum_value[domain_row];
    result.maximum_value = domains.maximum_value[domain_row];
    result.signed_domain =
        domains.signedness[domain_row] == lispb::schema::EnumSignedness::unspecified
            ? std::nullopt
            : std::optional{domains.signedness[domain_row] ==
                            lispb::schema::EnumSignedness::signed_};
    result.minimum_required_bits = domains.minimum_required_bits[domain_row];
    result.effective_bit_width = result.declared_bit_width.has_value()
                                   ? result.declared_bit_width
                                   : result.minimum_required_bits;
    if (domains.minimum_required_bits[domain_row].has_value() &&
        result.effective_bit_width.has_value()) {
        result.semantic_width_can_represent_domain =
            *result.effective_bit_width >= *domains.minimum_required_bits[domain_row];
    }
    if (result.effective_bit_width.has_value() && *result.effective_bit_width < 64) {
        auto const semantic_codes{std::uint64_t{1} << *result.effective_bit_width};
        if (domains.distinct_code_count[domain_row] <= semantic_codes) {
            result.unused_semantic_codes = semantic_codes - domains.distinct_code_count[domain_row];
        }
    }
    auto const value_count{domains.values(domain).size()};
    result.enumerators.reserve(value_count);
    for (std::size_t index{}; index < value_count; ++index) {
        auto const enumerator{domains.values(domain)[index]};
        auto const& enumerators{types.declarations().enumerators};
        auto const row{enumerators.index(enumerator)};
        result.enumerators.push_back(
            {.name = std::string{types.strings().view(enumerators.name[row])},
             .sentinel = enumerators.sentinel[row] != 0,
             .count_sentinel = enumerator == domains.count_value[domain_row],
             .code = domains.code(domain, index)});
    }
    auto const issue_count{domains.issue_count(domain)};
    for (std::uint32_t issue{}; issue < issue_count; ++issue) {
        result.diagnostics.push_back(
            {.severity = domains.issue_severity(domain, issue) ==
                                 lispb::schema::EnumDomainIssueSeverity::error
                           ? DiagnosticSeverity::error
                           : DiagnosticSeverity::warning,
             .message = domains.issue_message(domain, issue)});
    }

    if (result.minimum_value.has_value() && result.maximum_value.has_value() &&
        result.minimum_required_bits.has_value()) {
        if (result.backing_facts.has_value() && result.backing_bits.has_value()) {
            result.backing_can_represent_domain = enum_domain_fits_backing(*result.minimum_value,
                                                                           *result.maximum_value,
                                                                           *result.backing_facts,
                                                                           *result.backing_bits);
            if (!result.backing_can_represent_domain.has_value()) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::warning,
                     "Integer range facts are unavailable for enum backing type '" + backing_type +
                         "'; backing fit is unknown."});
            } else if (!*result.backing_can_represent_domain) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Enum value domain " + lispb::schema::format_enum_code(*result.minimum_value) +
                         " .. " + lispb::schema::format_enum_code(*result.maximum_value) +
                         " does not fit backing type '" + backing_type + "'."});
            }
        }
        auto backing_code_bits{std::optional<std::uint64_t>{}};
        if (result.backing_facts.has_value() &&
            result.backing_facts->integer_signed == std::optional{false} &&
            result.backing_facts->unsigned_value_bits.has_value()) {
            backing_code_bits = *result.backing_facts->unsigned_value_bits;
        } else if (result.backing_facts.has_value() &&
                   result.backing_facts->integer_signed == std::optional{true}) {
            backing_code_bits = result.backing_bits;
        }
        if (backing_code_bits.has_value() && *backing_code_bits < 64) {
            auto const backing_codes{std::uint64_t{1} << *backing_code_bits};
            if (domains.distinct_code_count[domain_row] <= backing_codes) {
                result.unused_backing_codes =
                    backing_codes - domains.distinct_code_count[domain_row];
            }
        }
    }

    auto const backing_size{
        result.backing_facts.transform([](TypeFacts const& facts) { return facts.size_bytes; })};
    if (backing_size.has_value() && *backing_size != 0) {
        result.aggregate.total_storage_bytes = checked_multiply(*backing_size, element_count);
        if (!result.aggregate.total_storage_bytes.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Enum standalone backing aggregate storage overflows uint64."});
        }

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
                *memory.cache_line_bytes / *backing_size;
            result.aggregate.cache_line_straddling_elements =
                straddling_elements(*backing_size,
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
            result.aggregate.complete_elements_per_page = *memory.page_bytes / *backing_size;
            result.aggregate.page_straddling_elements =
                straddling_elements(*backing_size,
                                    element_count,
                                    *memory.page_bytes,
                                    result.aggregate.total_storage_bytes);
        }
    }
    result.aggregate.cache_capacity =
        cache_capacity_analysis(result.aggregate.total_storage_bytes, abi.memory_facts());
    return result;
}

auto Analyzer::compare_enum_targets(EnumDomainAnalysis const& first,
                                    EnumDomainAnalysis const& second) -> EnumTargetComparison {
    EnumTargetComparison result;
    result.first = first;
    result.second = second;
    for (auto const& diagnostic : first.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "First target enum: " + diagnostic.message});
    }
    for (auto const& diagnostic : second.diagnostics) {
        result.diagnostics.push_back(
            {diagnostic.severity, "Second target enum: " + diagnostic.message});
    }

    if (first.type != second.type) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Compared target analyses describe different enums."});
        return result;
    }
    if (first.aggregate.element_count != second.aggregate.element_count) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target enum analyses use different element counts."});
        return result;
    }
    if (first.backing_type != second.backing_type ||
        first.live_value_count != second.live_value_count ||
        first.reserved_value_count != second.reserved_value_count ||
        first.minimum_value != second.minimum_value ||
        first.maximum_value != second.maximum_value ||
        first.signed_domain != second.signed_domain ||
        first.declared_signedness != second.declared_signedness ||
        first.minimum_required_bits != second.minimum_required_bits ||
        first.declared_bit_width != second.declared_bit_width ||
        first.effective_bit_width != second.effective_bit_width ||
        first.semantic_width_can_represent_domain != second.semantic_width_can_represent_domain ||
        first.unused_semantic_codes != second.unused_semantic_codes) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target enum analyses do not describe the same semantic domain and C++ "
             "backing choice."});
        return result;
    }

    StringSet first_names;
    StringSet second_names;
    for (auto const& enumerator : first.enumerators) {
        first_names.insert(enumerator.name);
    }
    for (auto const& enumerator : second.enumerators) {
        second_names.insert(enumerator.name);
    }
    if (first_names.size() != first.enumerators.size() ||
        second_names.size() != second.enumerators.size() || first_names != second_names) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Compared target enum analyses do not contain the same unique enumerators."});
        return result;
    }
    for (std::size_t index{}; index < first.enumerators.size(); ++index) {
        auto const& first_enumerator{first.enumerators[index]};
        auto const& second_enumerator{second.enumerators[index]};
        if (first_enumerator.name != second_enumerator.name ||
            first_enumerator.sentinel != second_enumerator.sentinel ||
            first_enumerator.count_sentinel != second_enumerator.count_sentinel ||
            first_enumerator.code != second_enumerator.code) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Compared target enum analyses do not contain the same ordered enumerators, "
                 "resolved codes, and reserved roles."});
            return result;
        }
    }

    auto const first_size{
        first.backing_facts.transform([](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const second_size{
        second.backing_facts.transform([](TypeFacts const& facts) { return facts.size_bytes; })};
    auto const first_alignment{first.backing_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    auto const second_alignment{second.backing_facts.transform(
        [](TypeFacts const& facts) { return facts.alignment_bytes; })};
    auto const first_value_bits{first.backing_facts.and_then([](TypeFacts const& facts) {
        return facts.unsigned_value_bits.transform(
            [](std::uint32_t const bits) { return static_cast<std::uint64_t>(bits); });
    })};
    auto const second_value_bits{second.backing_facts.and_then([](TypeFacts const& facts) {
        return facts.unsigned_value_bits.transform(
            [](std::uint32_t const bits) { return static_cast<std::uint64_t>(bits); });
    })};
    result.backing_size_delta = numeric_delta(first_size, second_size);
    result.backing_alignment_delta = numeric_delta(first_alignment, second_alignment);
    result.backing_value_bit_delta = numeric_delta(first_value_bits, second_value_bits);
    result.backing_bit_delta = numeric_delta(first.backing_bits, second.backing_bits);
    result.unused_backing_code_delta =
        numeric_delta(first.unused_backing_codes, second.unused_backing_codes);
    if (first.backing_can_represent_domain.has_value() &&
        second.backing_can_represent_domain.has_value()) {
        result.backing_fit_changed =
            *first.backing_can_represent_domain != *second.backing_can_represent_domain;
    }
    result.total_storage_delta =
        numeric_delta(first.aggregate.total_storage_bytes, second.aggregate.total_storage_bytes);
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

auto Analyzer::analyze_integer_scalar(
    lispb::schema::TypeGraph const& types,
    lispb::schema::TypeId const type,
    std::span<RelationshipTargetFacts const> const relationship_targets) -> IntegerScalarAnalysis {
    auto const domain{lispb::schema::integer_domain(types, type)};
    if (!domain.has_value()) {
        throw std::invalid_argument{"Integer scalar analysis requires a declared integer domain"};
    }
    auto const& scalar{*domain};
    auto required_minimum{
        types.definitions()
            .integer_scalars.minimum_value[types.definitions().integer_scalars.index(scalar)]};
    auto required_maximum{
        types.definitions()
            .integer_scalars.maximum_value[types.definitions().integer_scalars.index(scalar)]};
    auto sentinel_count{std::uint64_t{0}};
    std::vector<PackedNamedCodeAnalysis> named_codes;
    named_codes.reserve(
        types.definitions()
            .integer_scalars.named_codes[types.definitions().integer_scalars.index(scalar)]
            .size());
    for (auto const code_id : types.declarations().items(
             types.definitions()
                 .integer_scalars.named_codes[types.definitions().integer_scalars.index(scalar)])) {
        auto const code{code_id};
        named_codes.push_back(
            {.name = std::string{types.strings().view(
                 types.declarations()
                     .packed_named_codes
                     .name[types.declarations().packed_named_codes.index(code)])},
             .value =
                 types.declarations()
                     .packed_named_codes.value[types.declarations().packed_named_codes.index(code)],
             .sentinel = types.declarations()
                             .packed_named_codes
                             .sentinel[types.declarations().packed_named_codes.index(code)] != 0});
        if (types.declarations()
                .packed_named_codes.sentinel[types.declarations().packed_named_codes.index(code)]) {
            ++sentinel_count;
        }
        if (lispb::codegen::packed_integer_less(
                types.declarations()
                    .packed_named_codes.value[types.declarations().packed_named_codes.index(code)],
                required_minimum)) {
            required_minimum =
                types.declarations()
                    .packed_named_codes.value[types.declarations().packed_named_codes.index(code)];
        }
        if (lispb::codegen::packed_integer_less(
                required_maximum,
                types.declarations()
                    .packed_named_codes
                    .value[types.declarations().packed_named_codes.index(code)])) {
            required_maximum =
                types.declarations()
                    .packed_named_codes.value[types.declarations().packed_named_codes.index(code)];
        }
    }

    auto const live_count{packed_range_count(
        types.definitions()
            .integer_scalars.minimum_value[types.definitions().integer_scalars.index(scalar)],
        types.definitions()
            .integer_scalars.maximum_value[types.definitions().integer_scalars.index(scalar)])};
    auto const required_count{live_count.has_value() ? checked_add(*live_count, sentinel_count)
                                                     : std::nullopt};
    auto unused_codes{std::optional<std::uint64_t>{}};
    if (required_count.has_value()) {
        unused_codes =
            types.definitions().integer_scalars.bit_width[types.definitions().integer_scalars.index(
                scalar)] == 64
                ? std::optional<std::uint64_t>{maximum_analysis_count - *required_count + 1}
                : std::optional<std::uint64_t>{
                      (std::uint64_t{1}
                       << types.definitions()
                              .integer_scalars
                              .bit_width[types.definitions().integer_scalars.index(scalar)]) -
                      *required_count};
    } else if (types.definitions()
                       .integer_scalars
                       .bit_width[types.definitions().integer_scalars.index(scalar)] == 64 &&
               live_count.has_value() && sentinel_count > maximum_analysis_count - *live_count) {
        unused_codes = 0;
    } else if (sentinel_count == 0 &&
               types.definitions()
                       .integer_scalars
                       .bit_width[types.definitions().integer_scalars.index(scalar)] == 64) {
        auto const full_unsigned_domain{
            !types.definitions()
                 .integer_scalars.signedness[types.definitions().integer_scalars.index(scalar)] &&
            types.definitions()
                    .integer_scalars
                    .minimum_value[types.definitions().integer_scalars.index(scalar)] ==
                lispb::codegen::PackedIntegerValue{0} &&
            types.definitions()
                    .integer_scalars
                    .maximum_value[types.definitions().integer_scalars.index(scalar)] ==
                lispb::codegen::PackedIntegerValue{maximum_unsigned_code}};
        auto const full_signed_domain{
            types.definitions()
                .integer_scalars.signedness[types.definitions().integer_scalars.index(scalar)] &&
            types.definitions()
                    .integer_scalars
                    .minimum_value[types.definitions().integer_scalars.index(scalar)] ==
                lispb::codegen::PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63) &&
            types.definitions()
                    .integer_scalars
                    .maximum_value[types.definitions().integer_scalars.index(scalar)] ==
                lispb::codegen::PackedIntegerValue{maximum_signed_code}};
        if (full_unsigned_domain || full_signed_domain) {
            unused_codes = 0;
        }
    }

    auto result{IntegerScalarAnalysis{
        .type = type,
        .signedness =
            types.definitions()
                .integer_scalars.signedness[types.definitions().integer_scalars.index(scalar)] != 0,
        .minimum_value =
            types.definitions()
                .integer_scalars.minimum_value[types.definitions().integer_scalars.index(scalar)],
        .maximum_value =
            types.definitions()
                .integer_scalars.maximum_value[types.definitions().integer_scalars.index(scalar)],
        .live_value_count = live_count,
        .sentinel_code_count = sentinel_count,
        .required_code_count = required_count,
        .minimum_required_bits = *lispb::codegen::minimum_packed_integer_bits(
            required_minimum,
            required_maximum,
            types.definitions()
                .integer_scalars.signedness[types.definitions().integer_scalars.index(scalar)]),
        .declared_bit_width =
            types.definitions().integer_scalars.bit_width_auto[types.definitions()
                                                                   .integer_scalars.index(scalar)]
                ? std::nullopt
                : std::optional{types.definitions()
                                    .integer_scalars
                                    .bit_width[types.definitions().integer_scalars.index(scalar)]},
        .effective_bit_width =
            types.definitions()
                .integer_scalars.bit_width[types.definitions().integer_scalars.index(scalar)],
        .unused_codes = unused_codes,
        .named_codes = std::move(named_codes),
        .relationship_kind =
            lispb::schema::is_valid(
                types.definitions()
                    .integer_scalars
                    .relationship[types.definitions().integer_scalars.index(scalar)])
                ? std::optional{types.relationships().kind[types.relationships().index(
                      types.definitions().integer_scalars.relationship
                          [types.definitions().integer_scalars.index(scalar)])]}
                : std::nullopt,
        .relationship_unit =
            lispb::schema::is_valid(
                types.definitions()
                    .integer_scalars
                    .relationship[types.definitions().integer_scalars.index(scalar)])
                ? types.relationships().unit[types.relationships().index(
                      types.definitions().integer_scalars.relationship
                          [types.definitions().integer_scalars.index(scalar)])]
                : std::nullopt,
        .relationship_target =
            lispb::schema::is_valid(
                types.definitions()
                    .integer_scalars
                    .relationship[types.definitions().integer_scalars.index(scalar)])
                ? std::optional{std::string{types.strings().view(
                      types.name(types.occurrences().type[types.occurrences().index(
                          types.relationships().target[types.relationships().index(
                              types.definitions().integer_scalars.relationship
                                  [types.definitions().integer_scalars.index(scalar)])])]))}}
                : std::nullopt,
        .relationship_target_extent = std::nullopt,
        .relationship_live_value_count = std::nullopt,
        .relationship_required_code_count = std::nullopt,
        .relationship_minimum_required_bits = std::nullopt,
        .relationship_width_sufficient = std::nullopt,
        .relationship_code_space_capacity_limit = std::nullopt,
        .relationship_capacity_headroom = std::nullopt,
        .relationship_semantic_capacity_limit = std::nullopt,
        .relationship_sentinel_capacity_limit = std::nullopt,
        .relationship_effective_capacity_limit = std::nullopt,
        .relationship_effective_capacity_headroom = std::nullopt,
        .diagnostics = {}}};
    if (lispb::schema::is_valid(
            types.definitions()
                .integer_scalars.relationship[types.definitions().integer_scalars.index(scalar)]) &&
        (types.relationships().kind[types.relationships().index(
             types.definitions().integer_scalars.relationship
                 [types.definitions().integer_scalars.index(scalar)])] ==
             lispb::codegen::SemanticRelationKind::index_into ||
         types.relationships().kind[types.relationships().index(
             types.definitions().integer_scalars.relationship
                 [types.definitions().integer_scalars.index(scalar)])] ==
             lispb::codegen::SemanticRelationKind::count_of ||
         types.relationships().kind[types.relationships().index(
             types.definitions().integer_scalars.relationship
                 [types.definitions().integer_scalars.index(scalar)])] ==
             lispb::codegen::SemanticRelationKind::offset_into)) {
        auto const target_facts{
            std::ranges::find(relationship_targets,
                              types.occurrences().type[types.occurrences().index(
                                  types.relationships().target[types.relationships().index(
                                      types.definitions().integer_scalars.relationship
                                          [types.definitions().integer_scalars.index(scalar)])])],
                              &RelationshipTargetFacts::target)};
        if (target_facts != relationship_targets.end()) {
            auto const target_extent{relationship_target_extent(
                types,
                types.definitions()
                    .integer_scalars
                    .relationship[types.definitions().integer_scalars.index(scalar)],
                *target_facts)};
            if (!target_extent.has_value()) {
                return result;
            }
            auto const extent_analysis{analyze_unsigned_relationship_extent(
                types.declarations(),
                "Integer scalar '" + std::string{types.strings().view(types.name(type))} + "'",
                types.relationships().kind[types.relationships().index(
                    types.definitions().integer_scalars.relationship
                        [types.definitions().integer_scalars.index(scalar)])],
                *target_extent,
                types.definitions()
                    .integer_scalars.named_codes[types.definitions().integer_scalars.index(scalar)],
                types.definitions()
                    .integer_scalars.bit_width[types.definitions().integer_scalars.index(scalar)],
                types.definitions()
                    .integer_scalars
                    .minimum_value[types.definitions().integer_scalars.index(scalar)],
                types.definitions()
                    .integer_scalars
                    .maximum_value[types.definitions().integer_scalars.index(scalar)])};
            result.relationship_target_extent = extent_analysis.target_extent;
            result.relationship_live_value_count = extent_analysis.live_values;
            result.relationship_required_code_count = extent_analysis.required_codes;
            result.relationship_minimum_required_bits = extent_analysis.minimum_required_bits;
            result.relationship_width_sufficient = extent_analysis.width_sufficient;
            result.relationship_code_space_capacity_limit =
                extent_analysis.code_space_capacity_limit;
            result.relationship_capacity_headroom = extent_analysis.capacity_headroom;
            result.relationship_semantic_capacity_limit = extent_analysis.semantic_capacity_limit;
            result.relationship_sentinel_capacity_limit = extent_analysis.sentinel_capacity_limit;
            result.relationship_effective_capacity_limit = extent_analysis.effective_capacity_limit;
            result.relationship_effective_capacity_headroom =
                extent_analysis.effective_capacity_headroom;
            result.diagnostics = extent_analysis.diagnostics;
        }
    }
    return result;
}

auto Analyzer::compare_integer_scalar_capacity(
    lispb::schema::TypeGraph const& types,
    lispb::schema::TypeId const type,
    std::span<RelationshipTargetFacts const> const first_relationship_targets,
    std::span<RelationshipTargetFacts const> const second_relationship_targets)
    -> IntegerScalarCapacityComparison {
    auto first{analyze_integer_scalar(types, type, first_relationship_targets)};
    auto second{analyze_integer_scalar(types, type, second_relationship_targets)};
    auto const widen{[](std::optional<std::uint32_t> const value) {
        return value.transform(
            [](std::uint32_t const bits) { return static_cast<std::uint64_t>(bits); });
    }};
    auto fit_changed{std::optional<bool>{}};
    if (first.relationship_width_sufficient.has_value() &&
        second.relationship_width_sufficient.has_value()) {
        fit_changed = *first.relationship_width_sufficient != *second.relationship_width_sufficient;
    }
    auto const target_extent_delta{
        numeric_delta(first.relationship_target_extent, second.relationship_target_extent)};
    auto const minimum_width_delta{numeric_delta(widen(first.relationship_minimum_required_bits),
                                                 widen(second.relationship_minimum_required_bits))};
    auto const capacity_limit_delta{numeric_delta(first.relationship_code_space_capacity_limit,
                                                  second.relationship_code_space_capacity_limit)};
    auto const capacity_headroom_delta{
        numeric_delta(first.relationship_capacity_headroom, second.relationship_capacity_headroom)};
    auto const semantic_limit_delta{numeric_delta(first.relationship_semantic_capacity_limit,
                                                  second.relationship_semantic_capacity_limit)};
    auto const sentinel_limit_delta{numeric_delta(first.relationship_sentinel_capacity_limit,
                                                  second.relationship_sentinel_capacity_limit)};
    auto const effective_limit_delta{numeric_delta(first.relationship_effective_capacity_limit,
                                                   second.relationship_effective_capacity_limit)};
    auto const effective_headroom_delta{
        numeric_delta(first.relationship_effective_capacity_headroom,
                      second.relationship_effective_capacity_headroom)};

    return {.first = std::move(first),
            .second = std::move(second),
            .relationship_target_extent_delta = target_extent_delta,
            .relationship_minimum_required_bit_delta = minimum_width_delta,
            .relationship_code_space_capacity_limit_delta = capacity_limit_delta,
            .relationship_capacity_headroom_delta = capacity_headroom_delta,
            .relationship_semantic_capacity_limit_delta = semantic_limit_delta,
            .relationship_sentinel_capacity_limit_delta = sentinel_limit_delta,
            .relationship_effective_capacity_limit_delta = effective_limit_delta,
            .relationship_effective_capacity_headroom_delta = effective_headroom_delta,
            .relationship_width_fit_changed = fit_changed};
}

auto Analyzer::analyze_optional_sentinel(lispb::schema::TypeGraph const& types,
                                         lispb::schema::TypeId const type,
                                         std::uint64_t const element_count)
    -> OptionalSentinelAnalysis {
    auto const& optional{types.definition(type)};
    auto const source{*lispb::schema::integer_domain(
        types,
        types.occurrences().type[types.occurrences().index(
            types.definitions().optional_sentinels.source
                [types.definitions().optional_sentinels.index(optional)])])};
    auto const source_analysis{
        analyze_integer_scalar(types,
                               types.occurrences().type[types.occurrences().index(
                                   types.definitions().optional_sentinels.source
                                       [types.definitions().optional_sentinels.index(optional)])])};

    OptionalSentinelAnalysis result{
        .type = type,
        .source_type = types.occurrences().type[types.occurrences().index(
            types.definitions().optional_sentinels.source
                [types.definitions().optional_sentinels.index(optional)])],
        .sentinel_name = std::string{types.cpp_types().strings().view(
            types.definitions()
                .optional_sentinels
                .sentinel_name[types.definitions().optional_sentinels.index(optional)])},
        .sentinel_value =
            types.definitions()
                .optional_sentinels
                .sentinel_value[types.definitions().optional_sentinels.index(optional)],
        .source_signedness =
            types.definitions()
                .integer_scalars.signedness[types.definitions().integer_scalars.index(source)] != 0,
        .source_minimum =
            types.definitions()
                .integer_scalars.minimum_value[types.definitions().integer_scalars.index(source)],
        .source_maximum =
            types.definitions()
                .integer_scalars.maximum_value[types.definitions().integer_scalars.index(source)],
        .present_value_count = source_analysis.live_value_count,
        .absence_code_count = 1,
        .other_sentinel_code_count = source_analysis.sentinel_code_count - 1,
        .unused_code_count = source_analysis.unused_codes,
        .encoded_storage_bits =
            types.definitions()
                .optional_sentinels
                .bit_width[types.definitions().optional_sentinels.index(optional)],
        .total_code_count =
            types.definitions()
                        .optional_sentinels
                        .bit_width[types.definitions().optional_sentinels.index(optional)] == 64
                ? ExactCodeCount{.value = 0, .two_to_64 = true}
                : ExactCodeCount{.value =
                                     std::uint64_t{1}
                                     << types.definitions()
                                            .optional_sentinels
                                            .bit_width[types.definitions().optional_sentinels.index(
                                                optional)],
                                 .two_to_64 = false},
        .element_count = element_count,
        .total_encoded_bits =
            checked_multiply(types.definitions()
                                 .optional_sentinels
                                 .bit_width[types.definitions().optional_sentinels.index(optional)],
                             element_count),
        .diagnostics = {},
    };
    if (!result.total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Optional sentinel aggregate encoded-bit total exceeds the supported 64-bit range."});
    }
    return result;
}

auto Analyzer::analyze_optional_presence_bit(lispb::schema::TypeGraph const& types,
                                             lispb::schema::TypeId const type,
                                             std::uint64_t const element_count)
    -> OptionalPresenceBitAnalysis {
    auto const& optional{types.definition(type)};
    auto const source{*lispb::schema::integer_domain(
        types,
        types.occurrences().type[types.occurrences().index(
            types.definitions().optional_presence_bits.source
                [types.definitions().optional_presence_bits.index(optional)])])};
    auto const source_analysis{analyze_integer_scalar(
        types,
        types.occurrences().type[types.occurrences().index(
            types.definitions().optional_presence_bits.source
                [types.definitions().optional_presence_bits.index(optional)])])};

    OptionalPresenceBitAnalysis result{
        .type = type,
        .source_type = types.occurrences().type[types.occurrences().index(
            types.definitions().optional_presence_bits.source
                [types.definitions().optional_presence_bits.index(optional)])],
        .source_signedness =
            types.definitions()
                .integer_scalars.signedness[types.definitions().integer_scalars.index(source)] != 0,
        .source_minimum =
            types.definitions()
                .integer_scalars.minimum_value[types.definitions().integer_scalars.index(source)],
        .source_maximum =
            types.definitions()
                .integer_scalars.maximum_value[types.definitions().integer_scalars.index(source)],
        .present_value_count = source_analysis.live_value_count,
        .canonical_absence_state_count = 1,
        .source_sentinel_code_count = source_analysis.sentinel_code_count,
        .source_unused_payload_codes = source_analysis.unused_codes,
        .presence_bits = 1,
        .payload_bits =
            types.definitions()
                .optional_presence_bits
                .payload_bits[types.definitions().optional_presence_bits.index(optional)],
        .encoded_storage_bits =
            types.definitions()
                .optional_presence_bits
                .encoded_bits[types.definitions().optional_presence_bits.index(optional)],
        .noncanonical_absence_patterns =
            types.definitions()
                        .optional_presence_bits
                        .payload_bits[types.definitions().optional_presence_bits.index(optional)] ==
                    64
                ? maximum_analysis_count
                : (std::uint64_t{1}
                   << types.definitions().optional_presence_bits.payload_bits
                          [types.definitions().optional_presence_bits.index(optional)]) -
                      1,
        .element_count = element_count,
        .total_encoded_bits = checked_multiply(
            types.definitions()
                .optional_presence_bits
                .encoded_bits[types.definitions().optional_presence_bits.index(optional)],
            element_count),
        .diagnostics = {},
    };
    if (!result.total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Optional presence-bit aggregate encoded-bit total exceeds the supported 64-bit "
             "range."});
    }
    return result;
}

auto Analyzer::compare_optional_encodings(lispb::schema::TypeGraph const& types,
                                          lispb::schema::TypeId const first,
                                          lispb::schema::TypeId const second,
                                          std::uint64_t const element_count)
    -> OptionalEncodingComparison {
    OptionalEncodingComparison result{};
    result.element_count = element_count;

    auto summarize =
        [&](lispb::schema::TypeId const type) -> std::optional<OptionalEncodingSummary> {
        auto const& definition{types.definition(type)};
        if (((definition).kind() == lispb::schema::TypeDefinitionKind::optional_sentinel)) {
            auto const analysis{analyze_optional_sentinel(types, type, element_count)};
            result.diagnostics.insert(
                result.diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
            return OptionalEncodingSummary{
                .type = type,
                .source_type = analysis.source_type,
                .kind = OptionalEncodingKind::sentinel,
                .absence_sentinel_name = analysis.sentinel_name,
                .present_value_count = analysis.present_value_count,
                .canonical_absence_state_count = analysis.absence_code_count,
                .source_sentinel_code_count = analysis.other_sentinel_code_count + 1,
                .source_sentinel_codes_used_for_absence = 1,
                .remaining_source_sentinel_code_count = analysis.other_sentinel_code_count,
                .source_unused_payload_codes = analysis.unused_code_count,
                .noncanonical_absence_patterns = 0,
                .encoded_storage_bits = analysis.encoded_storage_bits,
                .total_encoded_bits = analysis.total_encoded_bits,
            };
        }
        if (((definition).kind() == lispb::schema::TypeDefinitionKind::optional_presence_bit)) {
            auto const analysis{analyze_optional_presence_bit(types, type, element_count)};
            result.diagnostics.insert(
                result.diagnostics.end(), analysis.diagnostics.begin(), analysis.diagnostics.end());
            return OptionalEncodingSummary{
                .type = type,
                .source_type = analysis.source_type,
                .kind = OptionalEncodingKind::presence_bit,
                .absence_sentinel_name = std::nullopt,
                .present_value_count = analysis.present_value_count,
                .canonical_absence_state_count = analysis.canonical_absence_state_count,
                .source_sentinel_code_count = analysis.source_sentinel_code_count,
                .source_sentinel_codes_used_for_absence = 0,
                .remaining_source_sentinel_code_count = analysis.source_sentinel_code_count,
                .source_unused_payload_codes = analysis.source_unused_payload_codes,
                .noncanonical_absence_patterns = analysis.noncanonical_absence_patterns,
                .encoded_storage_bits = analysis.encoded_storage_bits,
                .total_encoded_bits = analysis.total_encoded_bits,
            };
        }
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Type '" + std::string{types.strings().view(types.name(type))} +
                 "' is not a sentinel or presence-bit optional representation."});
        return std::nullopt;
    };

    auto const first_summary{summarize(first)};
    auto const second_summary{summarize(second)};
    result.supported_encodings = first_summary.has_value() && second_summary.has_value();
    if (!result.supported_encodings) {
        return result;
    }
    result.first = *first_summary;
    result.second = *second_summary;
    result.compatible_source = result.first.source_type == result.second.source_type;
    if (!result.compatible_source) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Optional representations '" + std::string{types.strings().view(types.name(first))} +
                 "' and '" + std::string{types.strings().view(types.name(second))} +
                 "' have different semantic sources and cannot be compared."});
        return result;
    }

    result.encoded_storage_bit_delta =
        numeric_delta(result.first.encoded_storage_bits, result.second.encoded_storage_bits);
    result.total_encoded_bit_delta =
        numeric_delta(result.first.total_encoded_bits, result.second.total_encoded_bits);
    return result;
}

auto Analyzer::analyze_linear_quantized(lispb::schema::TypeGraph const& types,
                                        lispb::schema::TypeId const type)
    -> LinearQuantizedAnalysis {
    auto const& quantized{types.definition(type)};
    auto const source{*lispb::schema::integer_domain(
        types,
        types.occurrences().type[types.occurrences().index(
            types.definitions()
                .linear_quantized.source[types.definitions().linear_quantized.index(quantized)])])};

    auto const total_code_count{
        types.definitions().linear_quantized.bit_width[types.definitions().linear_quantized.index(
            quantized)] == 64
            ? ExactCodeCount{.value = 0, .two_to_64 = true}
            : ExactCodeCount{
                  .value = std::uint64_t{1}
                        << types.definitions()
                               .linear_quantized
                               .bit_width[types.definitions().linear_quantized.index(quantized)],
                  .two_to_64 = false}};
    auto const usable_code_count{
        total_code_count.two_to_64 &&
                types.definitions()
                        .linear_quantized
                        .reserved_codes[types.definitions().linear_quantized.index(quantized)] == 0
            ? total_code_count
            : ExactCodeCount{
                  .value = total_code_count.two_to_64
                             ? maximum_analysis_count -
                                   types.definitions().linear_quantized.reserved_codes
                                       [types.definitions().linear_quantized.index(quantized)] +
                                   1
                             : total_code_count.value -
                                   types.definitions().linear_quantized.reserved_codes
                                       [types.definitions().linear_quantized.index(quantized)],
                  .two_to_64 = false}};

    auto const source_span{packed_range_span(
        types.definitions()
            .integer_scalars.minimum_value[types.definitions().integer_scalars.index(source)],
        types.definitions()
            .integer_scalars.maximum_value[types.definitions().integer_scalars.index(source)])};
    auto const numerical_code_count{std::ldexp(
        1.0L,
        static_cast<int>(types.definitions()
                             .linear_quantized
                             .bit_width[types.definitions().linear_quantized.index(quantized)]))};
    auto const numerical_usable_count{
        numerical_code_count -
        static_cast<long double>(
            types.definitions()
                .linear_quantized
                .reserved_codes[types.definitions().linear_quantized.index(quantized)])};
    auto const numerical_span{
        source_span.has_value()
            ? static_cast<long double>(*source_span)
            : packed_integer_as_long_double(
                  types.definitions()
                      .integer_scalars
                      .maximum_value[types.definitions().integer_scalars.index(source)]) -
                  packed_integer_as_long_double(
                      types.definitions()
                          .integer_scalars
                          .minimum_value[types.definitions().integer_scalars.index(source)])};
    auto const resolution{numerical_span / (numerical_usable_count - 1.0L)};

    return {
        .type = type,
        .source_type = types.occurrences().type[types.occurrences().index(
            types.definitions()
                .linear_quantized.source[types.definitions().linear_quantized.index(quantized)])],
        .source_minimum =
            types.definitions()
                .integer_scalars.minimum_value[types.definitions().integer_scalars.index(source)],
        .source_maximum =
            types.definitions()
                .integer_scalars.maximum_value[types.definitions().integer_scalars.index(source)],
        .source_span = source_span,
        .encoded_storage_bits =
            types.definitions()
                .linear_quantized.bit_width[types.definitions().linear_quantized.index(quantized)],
        .total_code_count = total_code_count,
        .reserved_code_count =
            types.definitions()
                .linear_quantized
                .reserved_codes[types.definitions().linear_quantized.index(quantized)],
        .usable_code_count = usable_code_count,
        .resolution = resolution,
        .maximum_rounding_error = resolution / 2.0L,
        .minimum_endpoint_exact = true,
        .maximum_endpoint_exact = true,
        .clipping =
            types.definitions()
                .linear_quantized.clipping[types.definitions().linear_quantized.index(quantized)]};
}

auto Analyzer::compare_linear_quantized(lispb::schema::TypeGraph const& types,
                                        lispb::schema::TypeId const first,
                                        lispb::schema::TypeId const second,
                                        std::uint64_t const element_count)
    -> LinearQuantizedComparison {
    LinearQuantizedComparison result{};
    result.first = analyze_linear_quantized(types, first);
    result.second = analyze_linear_quantized(types, second);
    result.element_count = element_count;
    result.compatible_source = result.first.source_type == result.second.source_type;
    if (!result.compatible_source) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Linear quantized representations '" +
                 std::string{types.strings().view(types.name(first))} + "' and '" +
                 std::string{types.strings().view(types.name(second))} +
                 "' have different semantic sources and cannot be compared."});
        return result;
    }

    result.encoded_storage_bit_delta =
        numeric_delta(result.first.encoded_storage_bits, result.second.encoded_storage_bits);
    result.total_code_count_delta =
        exact_code_count_delta(result.first.total_code_count, result.second.total_code_count);
    result.usable_code_count_delta =
        exact_code_count_delta(result.first.usable_code_count, result.second.usable_code_count);
    result.reserved_code_count_delta =
        numeric_delta(result.first.reserved_code_count, result.second.reserved_code_count);
    result.clipping_changed = result.first.clipping != result.second.clipping;
    result.resolution_delta = result.second.resolution - result.first.resolution;
    result.maximum_rounding_error_delta =
        result.second.maximum_rounding_error - result.first.maximum_rounding_error;
    result.first_total_encoded_bits =
        checked_multiply(result.first.encoded_storage_bits, element_count);
    result.second_total_encoded_bits =
        checked_multiply(result.second.encoded_storage_bits, element_count);
    if (!result.first_total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "First encoded payload bit count overflows uint64."});
    }
    if (!result.second_total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Second encoded payload bit count overflows uint64."});
    }
    result.total_encoded_bit_delta =
        numeric_delta(result.first_total_encoded_bits, result.second_total_encoded_bits);
    return result;
}

auto Analyzer::analyze_fixed_point(lispb::schema::TypeGraph const& types,
                                   lispb::schema::TypeId const type,
                                   std::uint64_t const element_count) -> FixedPointAnalysis {
    auto const& fixed{types.definition(type)};
    auto const sign_bits{
        types.definitions().fixed_points.signedness[types.definitions().fixed_points.index(fixed)]
            ? std::uint32_t{1}
            : std::uint32_t{0}};
    auto const whole_bits{
        types.definitions().fixed_points.total_bits[types.definitions().fixed_points.index(fixed)] -
        types.definitions()
            .fixed_points.fractional_bits[types.definitions().fixed_points.index(fixed)] -
        sign_bits};
    auto const resolution{std::ldexp(
        1.0L,
        -static_cast<int>(
            types.definitions()
                .fixed_points.fractional_bits[types.definitions().fixed_points.index(fixed)]))};
    auto const scale{std::ldexp(
        1.0L,
        static_cast<int>(
            types.definitions()
                .fixed_points.fractional_bits[types.definitions().fixed_points.index(fixed)]))};

    lispb::codegen::PackedIntegerValue minimum_raw;
    lispb::codegen::PackedIntegerValue maximum_raw;
    long double minimum_value{};
    long double maximum_value{};
    if (types.definitions()
            .fixed_points.signedness[types.definitions().fixed_points.index(fixed)]) {
        auto const negative_magnitude{
            std::uint64_t{1}
            << (types.definitions()
                    .fixed_points.total_bits[types.definitions().fixed_points.index(fixed)] -
                1)};
        minimum_raw = lispb::codegen::PackedIntegerValue::from_parts(true, negative_magnitude);
        maximum_raw = lispb::codegen::PackedIntegerValue{negative_magnitude - 1};
        auto const range_limit{std::ldexp(
            1.0L,
            static_cast<int>(
                types.definitions()
                    .fixed_points.total_bits[types.definitions().fixed_points.index(fixed)] -
                1) -
                static_cast<int>(
                    types.definitions()
                        .fixed_points
                        .fractional_bits[types.definitions().fixed_points.index(fixed)]))};
        minimum_value = -range_limit;
        maximum_value = range_limit - resolution;
    } else {
        minimum_raw = lispb::codegen::PackedIntegerValue{0};
        maximum_raw = lispb::codegen::PackedIntegerValue{
            types.definitions()
                        .fixed_points.total_bits[types.definitions().fixed_points.index(fixed)] ==
                    64
                ? maximum_unsigned_code
                : (std::uint64_t{1}
                   << types.definitions()
                          .fixed_points.total_bits[types.definitions().fixed_points.index(fixed)]) -
                      1};
        maximum_value =
            std::ldexp(
                1.0L,
                static_cast<int>(
                    types.definitions()
                        .fixed_points.total_bits[types.definitions().fixed_points.index(fixed)]) -
                    static_cast<int>(
                        types.definitions()
                            .fixed_points
                            .fractional_bits[types.definitions().fixed_points.index(fixed)])) -
            resolution;
    }

    auto const minimum_allowed_raw{
        types.definitions()
            .fixed_points.minimum_raw_value[types.definitions().fixed_points.index(fixed)]
            .value_or(minimum_raw)};
    auto const maximum_allowed_raw{
        types.definitions()
            .fixed_points.maximum_raw_value[types.definitions().fixed_points.index(fixed)]
            .value_or(maximum_raw)};
    auto const to_value = [&](lispb::codegen::PackedIntegerValue const raw) {
        auto const value{static_cast<long double>(raw.magnitude) / scale};
        return raw.negative ? -value : value;
    };
    auto const total_encoded_bits{checked_multiply(
        types.definitions().fixed_points.total_bits[types.definitions().fixed_points.index(fixed)],
        element_count)};
    FixedPointAnalysis result{
        .type = type,
        .signedness =
            types.definitions()
                .fixed_points.signedness[types.definitions().fixed_points.index(fixed)] != 0,
        .total_bits = types.definitions()
                          .fixed_points.total_bits[types.definitions().fixed_points.index(fixed)],
        .fractional_bits =
            types.definitions()
                .fixed_points.fractional_bits[types.definitions().fixed_points.index(fixed)],
        .whole_bits = whole_bits,
        .minimum_raw_value = minimum_raw,
        .maximum_raw_value = maximum_raw,
        .minimum_allowed_raw_value = minimum_allowed_raw,
        .maximum_allowed_raw_value = maximum_allowed_raw,
        .scale = scale,
        .resolution = resolution,
        .minimum_value = minimum_value,
        .maximum_value = maximum_value,
        .minimum_allowed_value = to_value(minimum_allowed_raw),
        .maximum_allowed_value = to_value(maximum_allowed_raw),
        .maximum_rounding_error =
            types.definitions()
                        .fixed_points.rounding[types.definitions().fixed_points.index(fixed)] ==
                    lispb::codegen::FixedPointRounding::nearest_even
                ? resolution / 2.0L
                : resolution,
        .rounding = types.definitions()
                        .fixed_points.rounding[types.definitions().fixed_points.index(fixed)],
        .element_count = element_count,
        .total_encoded_bits = total_encoded_bits,
        .diagnostics = {}};
    if (!total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Fixed-point encoded payload bit count overflows uint64."});
    }
    return result;
}

auto Analyzer::analyze_mini_float(lispb::schema::TypeGraph const& types,
                                  lispb::schema::TypeId const type,
                                  std::uint64_t const element_count) -> MiniFloatAnalysis {
    auto const& mini{types.definition(type)};
    auto const total_bits{
        types.definitions().mini_floats.sign_bits[types.definitions().mini_floats.index(mini)] +
        types.definitions().mini_floats.exponent_bits[types.definitions().mini_floats.index(mini)] +
        types.definitions()
            .mini_floats.significand_bits[types.definitions().mini_floats.index(mini)]};
    auto const exponent_code_count{
        std::uint64_t{1}
        << types.definitions()
               .mini_floats.exponent_bits[types.definitions().mini_floats.index(mini)]};
    auto const normal_exponent_code_count{exponent_code_count - 2};
    auto const sign_code_count{
        types.definitions().mini_floats.sign_bits[types.definitions().mini_floats.index(mini)] == 0
            ? std::uint64_t{1}
            : std::uint64_t{2}};
    auto const significand_code_count{
        std::uint64_t{1}
        << types.definitions()
               .mini_floats.significand_bits[types.definitions().mini_floats.index(mini)]};
    auto const minimum_normal_exponent{
        std::int32_t{1} -
        types.definitions().mini_floats.exponent_bias[types.definitions().mini_floats.index(mini)]};
    auto const maximum_normal_exponent{
        static_cast<std::int32_t>(exponent_code_count - 2) -
        types.definitions().mini_floats.exponent_bias[types.definitions().mini_floats.index(mini)]};
    auto const total_encoded_bits{checked_multiply(total_bits, element_count)};

    MiniFloatAnalysis result{
        .type = type,
        .sign_bits =
            types.definitions().mini_floats.sign_bits[types.definitions().mini_floats.index(mini)],
        .exponent_bits =
            types.definitions()
                .mini_floats.exponent_bits[types.definitions().mini_floats.index(mini)],
        .significand_bits =
            types.definitions()
                .mini_floats.significand_bits[types.definitions().mini_floats.index(mini)],
        .total_bits = total_bits,
        .exponent_bias =
            types.definitions()
                .mini_floats.exponent_bias[types.definitions().mini_floats.index(mini)],
        .exponent_code_count = exponent_code_count,
        .normal_exponent_code_count = normal_exponent_code_count,
        .minimum_normal_exponent = minimum_normal_exponent,
        .maximum_normal_exponent = maximum_normal_exponent,
        .total_code_count = {.value = total_bits == 64 ? 0 : std::uint64_t{1} << total_bits,
                             .two_to_64 = total_bits == 64},
        .zero_code_count = sign_code_count,
        .infinity_code_count = sign_code_count,
        .nan_code_count = (significand_code_count - 1) * sign_code_count,
        .nonzero_subnormal_code_count = (significand_code_count - 1) * sign_code_count,
        .minimum_positive_subnormal = std::nullopt,
        .minimum_positive_normal = std::nullopt,
        .maximum_finite = std::nullopt,
        .minimum_finite = std::nullopt,
        .unit_interval_resolution = std::nullopt,
        .maximum_relative_rounding_error = std::ldexp(
            1.0L,
            -static_cast<int>(
                types.definitions()
                    .mini_floats.significand_bits[types.definitions().mini_floats.index(mini)]) -
                1),
        .element_count = element_count,
        .total_encoded_bits = total_encoded_bits,
        .diagnostics = {}};

    auto retain_positive_finite =
        [&](long double const value,
            std::string_view const description) -> std::optional<long double> {
        if (std::isfinite(value) && value > 0.0L) {
            return value;
        }
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             std::string{description} +
                 " is outside the host long-double numerical range; exponent facts remain exact."});
        return std::nullopt;
    };

    result.minimum_positive_normal = retain_positive_finite(
        std::ldexp(1.0L, minimum_normal_exponent), "Minimum positive normal value");
    if (types.definitions()
            .mini_floats.significand_bits[types.definitions().mini_floats.index(mini)] != 0) {
        result.minimum_positive_subnormal = retain_positive_finite(
            std::ldexp(1.0L,
                       minimum_normal_exponent -
                           static_cast<int>(
                               types.definitions()
                                   .mini_floats
                                   .significand_bits[types.definitions().mini_floats.index(mini)])),
            "Minimum positive subnormal value");
    }
    auto const maximum_significand{
        2.0L -
        std::ldexp(
            1.0L,
            -static_cast<int>(
                types.definitions()
                    .mini_floats.significand_bits[types.definitions().mini_floats.index(mini)]))};
    result.maximum_finite = retain_positive_finite(
        std::ldexp(maximum_significand, maximum_normal_exponent), "Maximum finite value");
    if (result.maximum_finite.has_value()) {
        result.minimum_finite =
            types.definitions()
                        .mini_floats.sign_bits[types.definitions().mini_floats.index(mini)] == 0
                ? std::optional<long double>{0.0L}
                : std::optional{-*result.maximum_finite};
    }
    if (types.definitions()
                .mini_floats.exponent_bias[types.definitions().mini_floats.index(mini)] >= 1 &&
        types.definitions()
                .mini_floats.exponent_bias[types.definitions().mini_floats.index(mini)] <=
            static_cast<std::int32_t>(exponent_code_count - 2)) {
        result.unit_interval_resolution = std::ldexp(
            1.0L,
            -static_cast<int>(
                types.definitions()
                    .mini_floats.significand_bits[types.definitions().mini_floats.index(mini)]));
    } else {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "1.0 is outside the normal exponent range, so unit-interval resolution is unknown."});
    }
    if (!total_encoded_bits.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Mini-float encoded payload bit count overflows uint64."});
    }
    return result;
}

auto Analyzer::analyze_integer_varint(lispb::schema::TypeGraph const& types,
                                      lispb::schema::TypeId const type,
                                      std::uint64_t const element_count) -> IntegerVarintAnalysis {
    auto const& varint{types.definition(type)};
    auto const source{*lispb::schema::integer_domain(
        types,
        types.occurrences().type[types.occurrences().index(
            types.definitions()
                .integer_varints.source[types.definitions().integer_varints.index(varint)])])};

    auto const minimum_endpoint_bytes{integer_varint_bytes(
        types.definitions()
            .integer_scalars.minimum_value[types.definitions().integer_scalars.index(source)],
        types.definitions()
            .integer_varints.encoding[types.definitions().integer_varints.index(varint)])};
    auto const maximum_endpoint_bytes{integer_varint_bytes(
        types.definitions()
            .integer_scalars.maximum_value[types.definitions().integer_scalars.index(source)],
        types.definitions()
            .integer_varints.encoding[types.definitions().integer_varints.index(varint)])};
    auto minimum_bytes{std::min(minimum_endpoint_bytes, maximum_endpoint_bytes)};
    auto const zero{lispb::codegen::PackedIntegerValue{0}};
    if (lispb::codegen::packed_integer_less_equal(
            types.definitions()
                .integer_scalars.minimum_value[types.definitions().integer_scalars.index(source)],
            zero) &&
        lispb::codegen::packed_integer_less_equal(
            zero,
            types.definitions()
                .integer_scalars
                .maximum_value[types.definitions().integer_scalars.index(source)])) {
        minimum_bytes = 1;
    }
    auto maximum_bytes{std::max(minimum_endpoint_bytes, maximum_endpoint_bytes)};
    for (auto const code_id : types.declarations().items(
             types.definitions()
                 .integer_scalars.named_codes[types.definitions().integer_scalars.index(source)])) {
        auto const code{code_id};
        auto const bytes{integer_varint_bytes(
            types.declarations()
                .packed_named_codes.value[types.declarations().packed_named_codes.index(code)],
            types.definitions()
                .integer_varints.encoding[types.definitions().integer_varints.index(varint)])};
        minimum_bytes = std::min(minimum_bytes, bytes);
        maximum_bytes = std::max(maximum_bytes, bytes);
    }

    IntegerVarintAnalysis result{
        .type = type,
        .source_type = types.occurrences().type[types.occurrences().index(
            types.definitions()
                .integer_varints.source[types.definitions().integer_varints.index(varint)])],
        .encoding =
            types.definitions()
                .integer_varints.encoding[types.definitions().integer_varints.index(varint)],
        .minimum_encoded_bytes = minimum_bytes,
        .maximum_encoded_bytes = maximum_bytes,
        .element_count = element_count,
        .minimum_total_bytes = checked_multiply(minimum_bytes, element_count),
        .maximum_total_bytes = checked_multiply(maximum_bytes, element_count),
        .diagnostics = {}};
    if (!result.minimum_total_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Minimum varint byte total overflows uint64."});
    }
    if (!result.maximum_total_bytes.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error, "Maximum varint byte total overflows uint64."});
    }
    return result;
}

auto Analyzer::compare_integer_varint(lispb::schema::TypeGraph const& types,
                                      lispb::schema::TypeId const first,
                                      lispb::schema::TypeId const second,
                                      std::uint64_t const element_count)
    -> IntegerVarintComparison {
    IntegerVarintComparison result{};
    result.first = analyze_integer_varint(types, first, element_count);
    result.second = analyze_integer_varint(types, second, element_count);
    result.compatible_source = result.first.source_type == result.second.source_type;
    if (!result.compatible_source) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Integer varint representations '" +
                 std::string{types.strings().view(types.name(first))} + "' and '" +
                 std::string{types.strings().view(types.name(second))} +
                 "' have different semantic sources and cannot be compared."});
        return result;
    }

    result.minimum_encoded_byte_delta =
        numeric_delta(result.first.minimum_encoded_bytes, result.second.minimum_encoded_bytes);
    result.maximum_encoded_byte_delta =
        numeric_delta(result.first.maximum_encoded_bytes, result.second.maximum_encoded_bytes);
    result.minimum_total_byte_delta =
        numeric_delta(result.first.minimum_total_bytes, result.second.minimum_total_bytes);
    result.maximum_total_byte_delta =
        numeric_delta(result.first.maximum_total_bytes, result.second.maximum_total_bytes);
    result.diagnostics = result.first.diagnostics;
    result.diagnostics.insert(result.diagnostics.end(),
                              result.second.diagnostics.begin(),
                              result.second.diagnostics.end());
    return result;
}

auto Analyzer::analyze_integer_varint_distribution(
    lispb::schema::TypeGraph const& types,
    lispb::schema::TypeId const type,
    std::span<IntegerVarintDistributionEntry const> const entries,
    std::uint64_t const selected_element_count) -> IntegerVarintDistributionAnalysis {
    auto const& varint{types.definition(type)};
    auto const source{*lispb::schema::integer_domain(
        types,
        types.occurrences().type[types.occurrences().index(
            types.definitions()
                .integer_varints.source[types.definitions().integer_varints.index(varint)])])};
    IntegerVarintDistributionAnalysis result{.type = type,
                                             .valid_entry_count = 0,
                                             .total_weight = 0,
                                             .total_encoded_bytes = 0,
                                             .expected_bytes_per_value = std::nullopt,
                                             .selected_element_count = selected_element_count,
                                             .expected_selected_bytes = std::nullopt,
                                             .entries = {},
                                             .diagnostics = {}};

    auto exact_weight_overflow_reported{false};
    auto exact_bytes_overflow_reported{false};
    auto numerical_weight{0.0L};
    auto numerical_bytes{0.0L};
    for (auto const& entry : entries) {
        auto const in_live_range{
            lispb::codegen::packed_integer_less_equal(
                types.definitions()
                    .integer_scalars
                    .minimum_value[types.definitions().integer_scalars.index(source)],
                entry.value) &&
            lispb::codegen::packed_integer_less_equal(
                entry.value,
                types.definitions()
                    .integer_scalars
                    .maximum_value[types.definitions().integer_scalars.index(source)])};
        auto const named{std::ranges::any_of(
            types.declarations().items(
                types.definitions()
                    .integer_scalars
                    .named_codes[types.definitions().integer_scalars.index(source)]),
            [&](auto const code_id) {
                return types.declarations()
                           .packed_named_codes
                           .value[types.declarations().packed_named_codes.index(code_id)] ==
                       entry.value;
            })};
        if (!in_live_range && !named) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Distribution value " +
                                              lispb::codegen::format_packed_integer(entry.value) +
                                              " is outside the semantic source domain."});
            continue;
        }

        ++result.valid_entry_count;
        auto const bytes{integer_varint_bytes(
            entry.value,
            types.definitions()
                .integer_varints.encoding[types.definitions().integer_varints.index(varint)])};
        auto const weighted_bytes{checked_multiply(entry.weight, bytes)};
        result.entries.push_back({.value = entry.value,
                                  .weight = entry.weight,
                                  .encoded_bytes = bytes,
                                  .weighted_encoded_bytes = weighted_bytes});
        numerical_weight += static_cast<long double>(entry.weight);
        numerical_bytes += static_cast<long double>(entry.weight) * bytes;

        if (result.total_weight.has_value()) {
            result.total_weight = checked_add(*result.total_weight, entry.weight);
            if (!result.total_weight.has_value() && !exact_weight_overflow_reported) {
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "Distribution weight total overflows uint64."});
                exact_weight_overflow_reported = true;
            }
        }
        if (result.total_encoded_bytes.has_value()) {
            result.total_encoded_bytes =
                weighted_bytes.has_value()
                    ? checked_add(*result.total_encoded_bytes, *weighted_bytes)
                    : std::nullopt;
            if (!result.total_encoded_bytes.has_value() && !exact_bytes_overflow_reported) {
                result.diagnostics.push_back({DiagnosticSeverity::error,
                                              "Distribution encoded byte total overflows uint64."});
                exact_bytes_overflow_reported = true;
            }
        }
    }

    if (numerical_weight > 0.0L) {
        result.expected_bytes_per_value = numerical_bytes / numerical_weight;
        result.expected_selected_bytes =
            *result.expected_bytes_per_value * static_cast<long double>(selected_element_count);
    } else {
        result.diagnostics.push_back(
            {DiagnosticSeverity::warning,
             "Expected varint size is Unknown because the distribution has no positive valid "
             "weight."});
    }
    return result;
}

auto Analyzer::compare_integer_varint_distribution(
    lispb::schema::TypeGraph const& types,
    lispb::schema::TypeId const first,
    lispb::schema::TypeId const second,
    std::span<IntegerVarintDistributionEntry const> const entries,
    std::uint64_t const selected_element_count) -> IntegerVarintDistributionComparison {
    auto const& first_varint{types.definition(first)};
    auto const& second_varint{types.definition(second)};
    IntegerVarintDistributionComparison result{
        .first = analyze_integer_varint_distribution(types, first, entries, selected_element_count),
        .second =
            analyze_integer_varint_distribution(types, second, entries, selected_element_count),
        .compatible_source = types.occurrences().type[types.occurrences().index(
                                 types.definitions().integer_varints.source
                                     [types.definitions().integer_varints.index(first_varint)])] ==
                             types.occurrences().type[types.occurrences().index(
                                 types.definitions().integer_varints.source
                                     [types.definitions().integer_varints.index(second_varint)])],
        .total_encoded_byte_delta = std::nullopt,
        .expected_bytes_per_value_delta = std::nullopt,
        .expected_selected_bytes_delta = std::nullopt,
        .diagnostics = {}};
    if (!result.compatible_source) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Integer varint distributions for '" +
                 std::string{types.strings().view(types.name(first))} + "' and '" +
                 std::string{types.strings().view(types.name(second))} +
                 "' have different semantic sources and cannot be compared."});
        return result;
    }

    result.total_encoded_byte_delta =
        numeric_delta(result.first.total_encoded_bytes, result.second.total_encoded_bytes);
    if (result.first.expected_bytes_per_value.has_value() &&
        result.second.expected_bytes_per_value.has_value()) {
        result.expected_bytes_per_value_delta =
            *result.second.expected_bytes_per_value - *result.first.expected_bytes_per_value;
    }
    if (result.first.expected_selected_bytes.has_value() &&
        result.second.expected_selected_bytes.has_value()) {
        result.expected_selected_bytes_delta =
            *result.second.expected_selected_bytes - *result.first.expected_selected_bytes;
    }
    result.diagnostics = result.first.diagnostics;
    result.diagnostics.insert(result.diagnostics.end(),
                              result.second.diagnostics.begin(),
                              result.second.diagnostics.end());
    return result;
}

} // namespace ioj::layout
