#include "analyzer_internal.hpp"

namespace ioj::layout {

auto Analyzer::analyze_enum(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId const type,
                            AbiProfile const& abi,
                            std::uint64_t const element_count) -> EnumDomainAnalysis {
    auto const& enumeration{std::get<lispb::schema::EnumType>(types.type(type).definition)};
    auto const backing_type{enumeration.underlying_type.has_value()
                                ? physical_type_spelling(types, enumeration.underlying_type->type)
                                      .value_or(enumeration.underlying_type->cpp_type.spelling)
                                : derived_enum_backing_type(enumeration).value_or("Unknown")};
    EnumDomainAnalysis result{.type = type,
                              .backing_type = backing_type,
                              .backing_facts = abi.find(backing_type),
                              .backing_bits = std::nullopt,
                              .live_value_count = 0,
                              .reserved_value_count = 0,
                              .minimum_value = std::nullopt,
                              .maximum_value = std::nullopt,
                              .signed_domain = std::nullopt,
                              .declared_signedness = enumeration.signedness,
                              .minimum_required_bits = std::nullopt,
                              .declared_bit_width = enumeration.bit_width,
                              .effective_bit_width = std::nullopt,
                              .semantic_width_can_represent_domain = std::nullopt,
                              .unused_semantic_codes = std::nullopt,
                              .backing_can_represent_domain = std::nullopt,
                              .unused_backing_codes = std::nullopt,
                              .enumerators = {},
                              .diagnostics = {},
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

    auto const domain{resolved_enum_domain(enumeration)};
    result.live_value_count = domain.live_value_count;
    result.reserved_value_count = domain.reserved_value_count;
    result.minimum_value = domain.minimum_value;
    result.maximum_value = domain.maximum_value;
    result.signed_domain = domain.signed_domain;
    result.minimum_required_bits = domain.minimum_required_bits;
    result.effective_bit_width =
        enumeration.bit_width.has_value() ? enumeration.bit_width : domain.minimum_required_bits;
    if (domain.minimum_required_bits.has_value() && result.effective_bit_width.has_value()) {
        result.semantic_width_can_represent_domain =
            *result.effective_bit_width >= *domain.minimum_required_bits;
    }
    if (result.effective_bit_width.has_value() && *result.effective_bit_width < 64) {
        auto const semantic_codes{std::uint64_t{1} << *result.effective_bit_width};
        if (domain.distinct_code_count <= semantic_codes) {
            result.unused_semantic_codes = semantic_codes - domain.distinct_code_count;
        }
    }
    result.enumerators.reserve(domain.values.size());
    for (std::size_t index{}; index < domain.values.size(); ++index) {
        auto const& value{domain.values[index]};
        auto const& enumerator{enumeration.enumerators[index]};
        result.enumerators.push_back({.name = value.name,
                                      .sentinel = enumerator.sentinel,
                                      .count_sentinel = enumerator.count_sentinel,
                                      .code = value.code});
    }
    for (auto const& issue : domain.issues) {
        result.diagnostics.push_back(
            {.severity = issue.severity == lispb::schema::EnumDomainIssueSeverity::error
                           ? DiagnosticSeverity::error
                           : DiagnosticSeverity::warning,
             .message = issue.message});
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
            if (domain.distinct_code_count <= backing_codes) {
                result.unused_backing_codes = backing_codes - domain.distinct_code_count;
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

    std::set<std::string, std::less<>> first_names;
    std::set<std::string, std::less<>> second_names;
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
    auto const& scalar{std::get<lispb::schema::IntegerScalarType>(types.type(type).definition)};
    auto required_minimum{scalar.minimum_value};
    auto required_maximum{scalar.maximum_value};
    auto sentinel_count{std::uint64_t{0}};
    std::vector<PackedNamedCodeAnalysis> named_codes;
    named_codes.reserve(scalar.named_codes.size());
    for (auto const& code : scalar.named_codes) {
        named_codes.push_back({.name = code.name, .value = code.value, .sentinel = code.sentinel});
        if (code.sentinel) {
            ++sentinel_count;
        }
        if (codegen::packed_integer_less(code.value, required_minimum)) {
            required_minimum = code.value;
        }
        if (codegen::packed_integer_less(required_maximum, code.value)) {
            required_maximum = code.value;
        }
    }

    auto const live_count{packed_range_count(scalar.minimum_value, scalar.maximum_value)};
    auto const required_count{live_count.has_value() ? checked_add(*live_count, sentinel_count)
                                                     : std::nullopt};
    auto unused_codes{std::optional<std::uint64_t>{}};
    if (required_count.has_value()) {
        unused_codes = scalar.bit_width == 64
                         ? std::optional<std::uint64_t>{std::numeric_limits<std::uint64_t>::max() -
                                                        *required_count + 1}
                         : std::optional<std::uint64_t>{(std::uint64_t{1} << scalar.bit_width) -
                                                        *required_count};
    } else if (scalar.bit_width == 64 && live_count.has_value() &&
               sentinel_count > (std::numeric_limits<std::uint64_t>::max)() - *live_count) {
        unused_codes = 0;
    } else if (sentinel_count == 0 && scalar.bit_width == 64) {
        auto const full_unsigned_domain{
            !scalar.signedness && scalar.minimum_value == codegen::PackedIntegerValue{0} &&
            scalar.maximum_value ==
                codegen::PackedIntegerValue{(std::numeric_limits<std::uint64_t>::max)()}};
        auto const full_signed_domain{
            scalar.signedness &&
            scalar.minimum_value ==
                codegen::PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63) &&
            scalar.maximum_value ==
                codegen::PackedIntegerValue{(std::numeric_limits<std::int64_t>::max)()}};
        if (full_unsigned_domain || full_signed_domain) {
            unused_codes = 0;
        }
    }

    auto result{IntegerScalarAnalysis{
        .type = type,
        .signedness = scalar.signedness,
        .minimum_value = scalar.minimum_value,
        .maximum_value = scalar.maximum_value,
        .live_value_count = live_count,
        .sentinel_code_count = sentinel_count,
        .required_code_count = required_count,
        .minimum_required_bits = *codegen::minimum_packed_integer_bits(
            required_minimum, required_maximum, scalar.signedness),
        .declared_bit_width =
            scalar.bit_width_auto ? std::nullopt : std::optional{scalar.bit_width},
        .effective_bit_width = scalar.bit_width,
        .unused_codes = unused_codes,
        .named_codes = std::move(named_codes),
        .relationship_kind = scalar.relationship.has_value()
                               ? std::optional{scalar.relationship->kind}
                               : std::nullopt,
        .relationship_unit =
            scalar.relationship.has_value() ? scalar.relationship->unit : std::nullopt,
        .relationship_target =
            scalar.relationship.has_value()
                ? std::optional{types.type(scalar.relationship->target.type).identity.name}
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
    if (scalar.relationship.has_value() &&
        (scalar.relationship->kind == codegen::SemanticRelationKind::index_into ||
         scalar.relationship->kind == codegen::SemanticRelationKind::count_of ||
         scalar.relationship->kind == codegen::SemanticRelationKind::offset_into)) {
        auto const target_facts{std::ranges::find(relationship_targets,
                                                  scalar.relationship->target.type,
                                                  &RelationshipTargetFacts::target)};
        if (target_facts != relationship_targets.end()) {
            auto const target_extent{
                relationship_target_extent(*scalar.relationship, *target_facts)};
            if (!target_extent.has_value()) {
                return result;
            }
            auto const extent_analysis{analyze_unsigned_relationship_extent(
                "Integer scalar '" + types.type(type).identity.name + "'",
                scalar.relationship->kind,
                *target_extent,
                scalar.named_codes,
                scalar.bit_width,
                scalar.minimum_value,
                scalar.maximum_value)};
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
    auto const& optional{
        std::get<lispb::schema::OptionalSentinelType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(optional.source.type).definition)};
    auto const source_analysis{analyze_integer_scalar(types, optional.source.type)};

    OptionalSentinelAnalysis result{
        .type = type,
        .source_type = optional.source.type,
        .sentinel_name = optional.sentinel_name,
        .sentinel_value = optional.sentinel_value,
        .source_signedness = source.signedness,
        .source_minimum = source.minimum_value,
        .source_maximum = source.maximum_value,
        .present_value_count = source_analysis.live_value_count,
        .absence_code_count = 1,
        .other_sentinel_code_count = source_analysis.sentinel_code_count - 1,
        .unused_code_count = source_analysis.unused_codes,
        .encoded_storage_bits = optional.bit_width,
        .total_code_count = optional.bit_width == 64
                              ? ExactCodeCount{.value = 0, .two_to_64 = true}
                              : ExactCodeCount{.value = std::uint64_t{1} << optional.bit_width,
                                               .two_to_64 = false},
        .element_count = element_count,
        .total_encoded_bits = checked_multiply(optional.bit_width, element_count),
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
    auto const& optional{
        std::get<lispb::schema::OptionalPresenceBitType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(optional.source.type).definition)};
    auto const source_analysis{analyze_integer_scalar(types, optional.source.type)};

    OptionalPresenceBitAnalysis result{
        .type = type,
        .source_type = optional.source.type,
        .source_signedness = source.signedness,
        .source_minimum = source.minimum_value,
        .source_maximum = source.maximum_value,
        .present_value_count = source_analysis.live_value_count,
        .canonical_absence_state_count = 1,
        .source_sentinel_code_count = source_analysis.sentinel_code_count,
        .source_unused_payload_codes = source_analysis.unused_codes,
        .presence_bits = 1,
        .payload_bits = optional.payload_bits,
        .encoded_storage_bits = optional.encoded_bits,
        .noncanonical_absence_patterns = optional.payload_bits == 64
                                           ? (std::numeric_limits<std::uint64_t>::max)()
                                           : (std::uint64_t{1} << optional.payload_bits) - 1,
        .element_count = element_count,
        .total_encoded_bits = checked_multiply(optional.encoded_bits, element_count),
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
        auto const& definition{types.type(type).definition};
        if (std::holds_alternative<lispb::schema::OptionalSentinelType>(definition)) {
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
        if (std::holds_alternative<lispb::schema::OptionalPresenceBitType>(definition)) {
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
             "Type '" + types.type(type).identity.name +
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
             "Optional representations '" + types.type(first).identity.name + "' and '" +
                 types.type(second).identity.name +
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
    auto const& quantized{
        std::get<lispb::schema::LinearQuantizedType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(quantized.source.type).definition)};

    auto const total_code_count{
        quantized.bit_width == 64
            ? ExactCodeCount{.value = 0, .two_to_64 = true}
            : ExactCodeCount{.value = std::uint64_t{1} << quantized.bit_width, .two_to_64 = false}};
    auto const usable_code_count{
        total_code_count.two_to_64 && quantized.reserved_codes == 0
            ? total_code_count
            : ExactCodeCount{.value = total_code_count.two_to_64
                                        ? (std::numeric_limits<std::uint64_t>::max)() -
                                              quantized.reserved_codes + 1
                                        : total_code_count.value - quantized.reserved_codes,
                             .two_to_64 = false}};

    auto const source_span{packed_range_span(source.minimum_value, source.maximum_value)};
    auto const numerical_code_count{std::ldexp(1.0L, static_cast<int>(quantized.bit_width))};
    auto const numerical_usable_count{numerical_code_count -
                                      static_cast<long double>(quantized.reserved_codes)};
    auto const numerical_span{source_span.has_value()
                                  ? static_cast<long double>(*source_span)
                                  : packed_integer_as_long_double(source.maximum_value) -
                                        packed_integer_as_long_double(source.minimum_value)};
    auto const resolution{numerical_span / (numerical_usable_count - 1.0L)};

    return {.type = type,
            .source_type = quantized.source.type,
            .source_minimum = source.minimum_value,
            .source_maximum = source.maximum_value,
            .source_span = source_span,
            .encoded_storage_bits = quantized.bit_width,
            .total_code_count = total_code_count,
            .reserved_code_count = quantized.reserved_codes,
            .usable_code_count = usable_code_count,
            .resolution = resolution,
            .maximum_rounding_error = resolution / 2.0L,
            .minimum_endpoint_exact = true,
            .maximum_endpoint_exact = true,
            .clipping = quantized.clipping};
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
             "Linear quantized representations '" + types.type(first).identity.name + "' and '" +
                 types.type(second).identity.name +
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
    auto const& fixed{std::get<lispb::schema::FixedPointType>(types.type(type).definition)};
    auto const sign_bits{fixed.signedness ? std::uint32_t{1} : std::uint32_t{0}};
    auto const whole_bits{fixed.total_bits - fixed.fractional_bits - sign_bits};
    auto const resolution{std::ldexp(1.0L, -static_cast<int>(fixed.fractional_bits))};
    auto const scale{std::ldexp(1.0L, static_cast<int>(fixed.fractional_bits))};

    codegen::PackedIntegerValue minimum_raw;
    codegen::PackedIntegerValue maximum_raw;
    long double minimum_value{};
    long double maximum_value{};
    if (fixed.signedness) {
        auto const negative_magnitude{std::uint64_t{1} << (fixed.total_bits - 1)};
        minimum_raw = codegen::PackedIntegerValue::from_parts(true, negative_magnitude);
        maximum_raw = codegen::PackedIntegerValue{negative_magnitude - 1};
        auto const range_limit{std::ldexp(1.0L,
                                          static_cast<int>(fixed.total_bits - 1) -
                                              static_cast<int>(fixed.fractional_bits))};
        minimum_value = -range_limit;
        maximum_value = range_limit - resolution;
    } else {
        minimum_raw = codegen::PackedIntegerValue{0};
        maximum_raw = codegen::PackedIntegerValue{fixed.total_bits == 64
                                                      ? (std::numeric_limits<std::uint64_t>::max)()
                                                      : (std::uint64_t{1} << fixed.total_bits) - 1};
        maximum_value = std::ldexp(1.0L,
                                   static_cast<int>(fixed.total_bits) -
                                       static_cast<int>(fixed.fractional_bits)) -
                        resolution;
    }

    auto const total_encoded_bits{checked_multiply(fixed.total_bits, element_count)};
    FixedPointAnalysis result{.type = type,
                              .signedness = fixed.signedness,
                              .total_bits = fixed.total_bits,
                              .fractional_bits = fixed.fractional_bits,
                              .whole_bits = whole_bits,
                              .minimum_raw_value = minimum_raw,
                              .maximum_raw_value = maximum_raw,
                              .scale = scale,
                              .resolution = resolution,
                              .minimum_value = minimum_value,
                              .maximum_value = maximum_value,
                              .maximum_rounding_error =
                                  fixed.rounding == codegen::FixedPointRounding::nearest_even
                                      ? resolution / 2.0L
                                      : resolution,
                              .rounding = fixed.rounding,
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
    auto const& mini{std::get<lispb::schema::MiniFloatType>(types.type(type).definition)};
    auto const total_bits{mini.sign_bits + mini.exponent_bits + mini.significand_bits};
    auto const exponent_code_count{std::uint64_t{1} << mini.exponent_bits};
    auto const normal_exponent_code_count{exponent_code_count - 2};
    auto const sign_code_count{mini.sign_bits == 0 ? std::uint64_t{1} : std::uint64_t{2}};
    auto const significand_code_count{std::uint64_t{1} << mini.significand_bits};
    auto const minimum_normal_exponent{std::int32_t{1} - mini.exponent_bias};
    auto const maximum_normal_exponent{static_cast<std::int32_t>(exponent_code_count - 2) -
                                       mini.exponent_bias};
    auto const total_encoded_bits{checked_multiply(total_bits, element_count)};

    MiniFloatAnalysis result{
        .type = type,
        .sign_bits = mini.sign_bits,
        .exponent_bits = mini.exponent_bits,
        .significand_bits = mini.significand_bits,
        .total_bits = total_bits,
        .exponent_bias = mini.exponent_bias,
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
        .maximum_relative_rounding_error =
            std::ldexp(1.0L, -static_cast<int>(mini.significand_bits) - 1),
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
    if (mini.significand_bits != 0) {
        result.minimum_positive_subnormal = retain_positive_finite(
            std::ldexp(1.0L, minimum_normal_exponent - static_cast<int>(mini.significand_bits)),
            "Minimum positive subnormal value");
    }
    auto const maximum_significand{2.0L -
                                   std::ldexp(1.0L, -static_cast<int>(mini.significand_bits))};
    result.maximum_finite = retain_positive_finite(
        std::ldexp(maximum_significand, maximum_normal_exponent), "Maximum finite value");
    if (result.maximum_finite.has_value()) {
        result.minimum_finite = mini.sign_bits == 0 ? std::optional<long double>{0.0L}
                                                    : std::optional{-*result.maximum_finite};
    }
    if (mini.exponent_bias >= 1 &&
        mini.exponent_bias <= static_cast<std::int32_t>(exponent_code_count - 2)) {
        result.unit_interval_resolution =
            std::ldexp(1.0L, -static_cast<int>(mini.significand_bits));
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
    auto const& varint{std::get<lispb::schema::IntegerVarintType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(varint.source.type).definition)};

    auto const minimum_endpoint_bytes{integer_varint_bytes(source.minimum_value, varint.encoding)};
    auto const maximum_endpoint_bytes{integer_varint_bytes(source.maximum_value, varint.encoding)};
    auto minimum_bytes{std::min(minimum_endpoint_bytes, maximum_endpoint_bytes)};
    auto const zero{codegen::PackedIntegerValue{0}};
    if (codegen::packed_integer_less_equal(source.minimum_value, zero) &&
        codegen::packed_integer_less_equal(zero, source.maximum_value)) {
        minimum_bytes = 1;
    }
    auto maximum_bytes{std::max(minimum_endpoint_bytes, maximum_endpoint_bytes)};
    for (auto const& code : source.named_codes) {
        auto const bytes{integer_varint_bytes(code.value, varint.encoding)};
        minimum_bytes = std::min(minimum_bytes, bytes);
        maximum_bytes = std::max(maximum_bytes, bytes);
    }

    IntegerVarintAnalysis result{
        .type = type,
        .source_type = varint.source.type,
        .encoding = varint.encoding,
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
             "Integer varint representations '" + types.type(first).identity.name + "' and '" +
                 types.type(second).identity.name +
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
    auto const& varint{std::get<lispb::schema::IntegerVarintType>(types.type(type).definition)};
    auto const& source{
        std::get<lispb::schema::IntegerScalarType>(types.type(varint.source.type).definition)};
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
            codegen::packed_integer_less_equal(source.minimum_value, entry.value) &&
            codegen::packed_integer_less_equal(entry.value, source.maximum_value)};
        auto const named{std::ranges::any_of(
            source.named_codes, [&](auto const& code) { return code.value == entry.value; })};
        if (!in_live_range && !named) {
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Distribution value " +
                                              codegen::format_packed_integer(entry.value) +
                                              " is outside the semantic source domain."});
            continue;
        }

        ++result.valid_entry_count;
        auto const bytes{integer_varint_bytes(entry.value, varint.encoding)};
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
    auto const& first_varint{
        std::get<lispb::schema::IntegerVarintType>(types.type(first).definition)};
    auto const& second_varint{
        std::get<lispb::schema::IntegerVarintType>(types.type(second).definition)};
    IntegerVarintDistributionComparison result{
        .first = analyze_integer_varint_distribution(types, first, entries, selected_element_count),
        .second =
            analyze_integer_varint_distribution(types, second, entries, selected_element_count),
        .compatible_source = first_varint.source.type == second_varint.source.type,
        .total_encoded_byte_delta = std::nullopt,
        .expected_bytes_per_value_delta = std::nullopt,
        .expected_selected_bytes_delta = std::nullopt,
        .diagnostics = {}};
    if (!result.compatible_source) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Integer varint distributions for '" + types.type(first).identity.name + "' and '" +
                 types.type(second).identity.name +
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
