#include "scalar_pass.hpp"

#include "../analysis_limits.hpp"
#include "../checked_arithmetic.hpp"
#include "../enum_backing.hpp"
#include "../integer_code_domain.hpp"
#include "../memory_regions.hpp"
#include "../relationship_extent.hpp"
#include "../varint_encoding.hpp"
#include <ioj/layout/abi_profile.hpp>
#include <ioj/layout/analysis/analysis_context.hpp>
#include <ioj/layout/analysis/analysis_storage.hpp>
#include <ioj/layout/analyzer/exact_code_count.hpp>
#include <ioj/layout/analyzer/integer_varint_distribution_entry.hpp>
#include <ioj/layout/analyzer/numeric_delta.hpp>
#include <ioj/layout/analyzer/physical_type_spelling.hpp>
#include <ioj/layout/string_set.hpp>
#include <ioj/layout/type_facts.hpp>

#include <codegen/ast/cpp_types.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/semantic_relation_kind.h>
#include <lispb/project_loader.h>
#include <lispb/schema/enum_domain_issue_severity.h>
#include <lispb/schema/enum_domains.h>
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
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
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

namespace ioj::layout::analysis_detail {
using lispb::codegen::PackedIntegerValue;

auto compute_integer_domain(AnalysisContext& context, lispb::schema::TypeId type) -> IntegerDomain {
    auto const& types{context.types};

    auto const domain{lispb::schema::integer_domain(types, type)};
    if (!domain.has_value()) {
        assert(false && "Resolved integer domain required");
    }
    auto const& scalar{*domain};
    auto const& integer_scalars{types.definitions().integer_scalars};
    auto const scalar_row{integer_scalars.index(scalar)};
    auto required_minimum{integer_scalars.minimum_value[scalar_row]};
    auto required_maximum{integer_scalars.maximum_value[scalar_row]};
    auto sentinel_count{std::uint64_t{0}};
    auto& codes{context.storage.codes};
    auto const code_begin{codes.num()};
    for (auto const code_id : types.declarations().items(integer_scalars.named_codes[scalar_row])) {
        auto const code{code_id};
        auto const named_row{types.declarations().packed_named_codes.index(code)};
        auto const code_row{codes.append()};
        codes.name()[code_row] =
            types.strings().view(types.declarations().packed_named_codes.name[named_row]);
        codes.integer_value()[code_row] = types.declarations().packed_named_codes.value[named_row];
        codes.sentinel()[code_row] = types.declarations().packed_named_codes.sentinel[named_row];
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

    auto const live_count{packed_range_count(integer_scalars.minimum_value[scalar_row],
                                             integer_scalars.maximum_value[scalar_row])};
    auto const required_count{live_count.has_value() ? checked_add(*live_count, sentinel_count)
                                                     : std::nullopt};
    auto unused_codes{std::optional<std::uint64_t>{}};
    if (required_count.has_value()) {
        unused_codes =
            types.definitions().integer_scalars.bit_width[types.definitions().integer_scalars.index(
                scalar)] == 64
                ? std::optional<std::uint64_t>{maximum_analysis_count - *required_count + 1}
                : std::optional<std::uint64_t>{
                      (std::uint64_t{1} << integer_scalars.bit_width[scalar_row]) -
                      *required_count};
    } else if (integer_scalars.bit_width[scalar_row] == 64 && live_count.has_value() &&
               sentinel_count > maximum_analysis_count - *live_count) {
        unused_codes = 0;
    } else if (sentinel_count == 0 && integer_scalars.bit_width[scalar_row] == 64) {
        auto const full_unsigned_domain{
            !integer_scalars.signedness[scalar_row] &&
            integer_scalars.minimum_value[scalar_row] == PackedIntegerValue{0} &&
            integer_scalars.maximum_value[scalar_row] == PackedIntegerValue{maximum_unsigned_code}};
        auto const full_signed_domain{
            integer_scalars.signedness[scalar_row] &&
            integer_scalars.minimum_value[scalar_row] ==
                PackedIntegerValue::from_parts(true, std::uint64_t{1} << 63) &&
            integer_scalars.maximum_value[scalar_row] == PackedIntegerValue{maximum_signed_code}};
        if (full_unsigned_domain || full_signed_domain) {
            unused_codes = 0;
        }
    }

    auto result{IntegerDomain{
        .signedness = integer_scalars.signedness[scalar_row] != 0,
        .minimum_value = integer_scalars.minimum_value[scalar_row],
        .maximum_value = integer_scalars.maximum_value[scalar_row],
        .live_value_count = live_count,
        .sentinel_code_count = sentinel_count,
        .required_code_count = required_count,
        .minimum_required_bits = *lispb::codegen::minimum_packed_integer_bits(
            required_minimum, required_maximum, integer_scalars.signedness[scalar_row]),
        .declared_bit_width = integer_scalars.bit_width_auto[scalar_row]
                                ? std::nullopt
                                : std::optional{integer_scalars.bit_width[scalar_row]},
        .effective_bit_width = integer_scalars.bit_width[scalar_row],
        .unused_codes = unused_codes,
        .codes = {code_begin, codes.num() - code_begin}}};
    return result;
}

auto compute_optional_sentinel_encoding(AnalysisContext& context, lispb::schema::TypeId type)
    -> OptionalSentinelEncoding {
    auto const& types{context.types};

    auto const& optional{types.definition(type)};
    auto const& optional_sentinels{types.definitions().optional_sentinels};
    auto const optional_row{optional_sentinels.index(optional)};
    auto const source{*lispb::schema::integer_domain(
        types,
        types.occurrences()
            .type[types.occurrences().index(optional_sentinels.source[optional_row])])};
    auto const& integer_scalars{types.definitions().integer_scalars};
    auto const source_row{integer_scalars.index(source)};
    auto const source_type{
        types.occurrences()
            .type[types.occurrences().index(optional_sentinels.source[optional_row])]};
    auto const& source_analysis{
        *context.storage.scalars.integer_domain()[context.storage.scalars.find(source_type)]};

    OptionalSentinelEncoding result{
        .source_type =
            types.occurrences()
                .type[types.occurrences().index(optional_sentinels.source[optional_row])],
        .sentinel_name = context.storage.strings.intern(
            types.cpp_types().strings().view(optional_sentinels.sentinel_name[optional_row])),
        .sentinel_value = optional_sentinels.sentinel_value[optional_row],
        .source_signedness = integer_scalars.signedness[source_row] != 0,
        .source_minimum = integer_scalars.minimum_value[source_row],
        .source_maximum = integer_scalars.maximum_value[source_row],
        .present_value_count = source_analysis.live_value_count,
        .other_sentinel_code_count = source_analysis.sentinel_code_count - 1,
        .unused_code_count = source_analysis.unused_codes,
        .encoded_storage_bits = optional_sentinels.bit_width[optional_row],
        .total_code_count =
            optional_sentinels.bit_width[optional_row] == 64
                ? ExactCodeCount{.two_to_64 = true}
                : ExactCodeCount{
                      .value =
                          std::uint64_t{1}
                          << types.definitions()
                                 .optional_sentinels
                                 .bit_width[types.definitions().optional_sentinels.index(optional)],
                      .two_to_64 = false}};
    return result;
}

auto compute_optional_presence_encoding(AnalysisContext& context, lispb::schema::TypeId type)
    -> OptionalPresenceEncoding {
    auto const& types{context.types};

    auto const& optional{types.definition(type)};
    auto const& optional_presence_bits{types.definitions().optional_presence_bits};
    auto const optional_row{optional_presence_bits.index(optional)};
    auto const source{*lispb::schema::integer_domain(
        types,
        types.occurrences()
            .type[types.occurrences().index(optional_presence_bits.source[optional_row])])};
    auto const& integer_scalars{types.definitions().integer_scalars};
    auto const source_row{integer_scalars.index(source)};
    auto const source_type{
        types.occurrences()
            .type[types.occurrences().index(optional_presence_bits.source[optional_row])]};
    auto const& source_analysis{
        *context.storage.scalars.integer_domain()[context.storage.scalars.find(source_type)]};

    OptionalPresenceEncoding result{
        .source_type =
            types.occurrences()
                .type[types.occurrences().index(optional_presence_bits.source[optional_row])],
        .source_signedness = integer_scalars.signedness[source_row] != 0,
        .source_minimum = integer_scalars.minimum_value[source_row],
        .source_maximum = integer_scalars.maximum_value[source_row],
        .present_value_count = source_analysis.live_value_count,
        .source_sentinel_code_count = source_analysis.sentinel_code_count,
        .source_unused_payload_codes = source_analysis.unused_codes,
        .payload_bits = optional_presence_bits.payload_bits[optional_row],
        .encoded_storage_bits = optional_presence_bits.encoded_bits[optional_row],
        .noncanonical_absence_patterns =
            optional_presence_bits.payload_bits[optional_row] == 64
                ? maximum_analysis_count
                : (std::uint64_t{1} << optional_presence_bits.payload_bits[optional_row]) - 1};
    return result;
}

auto compute_quantized_encoding(AnalysisContext& context, lispb::schema::TypeId type)
    -> QuantizedEncoding {
    auto const& types{context.types};

    auto const& quantized{types.definition(type)};
    auto const& linear_quantized{types.definitions().linear_quantized};
    auto const quantized_row{linear_quantized.index(quantized)};
    auto const source{*lispb::schema::integer_domain(
        types,
        types.occurrences()
            .type[types.occurrences().index(linear_quantized.source[quantized_row])])};
    auto const& integer_scalars{types.definitions().integer_scalars};
    auto const source_row{integer_scalars.index(source)};

    auto const total_code_count{
        types.definitions().linear_quantized.bit_width[types.definitions().linear_quantized.index(
            quantized)] == 64
            ? ExactCodeCount{.two_to_64 = true}
            : ExactCodeCount{.value = std::uint64_t{1} << linear_quantized.bit_width[quantized_row],
                             .two_to_64 = false}};
    auto const usable_code_count{
        total_code_count.two_to_64 && linear_quantized.reserved_codes[quantized_row] == 0
            ? total_code_count
            : ExactCodeCount{.value = total_code_count.two_to_64
                                        ? maximum_analysis_count -
                                              linear_quantized.reserved_codes[quantized_row] + 1
                                        : total_code_count.value -
                                              linear_quantized.reserved_codes[quantized_row],
                             .two_to_64 = false}};

    auto const source_span{packed_range_span(integer_scalars.minimum_value[source_row],
                                             integer_scalars.maximum_value[source_row])};
    auto const numerical_code_count{
        std::ldexp(1.0L, static_cast<int>(linear_quantized.bit_width[quantized_row]))};
    auto const numerical_usable_count{
        numerical_code_count -
        static_cast<long double>(linear_quantized.reserved_codes[quantized_row])};
    auto const numerical_span{
        source_span.has_value()
            ? static_cast<long double>(*source_span)
            : packed_integer_as_long_double(integer_scalars.maximum_value[source_row]) -
                  packed_integer_as_long_double(integer_scalars.minimum_value[source_row])};
    auto const resolution{numerical_span / (numerical_usable_count - 1.0L)};

    return {.source_type =
                types.occurrences()
                    .type[types.occurrences().index(linear_quantized.source[quantized_row])],
            .source_minimum = integer_scalars.minimum_value[source_row],
            .source_maximum = integer_scalars.maximum_value[source_row],
            .source_span = source_span,
            .encoded_storage_bits = linear_quantized.bit_width[quantized_row],
            .total_code_count = total_code_count,
            .reserved_code_count = linear_quantized.reserved_codes[quantized_row],
            .usable_code_count = usable_code_count,
            .resolution = resolution,
            .maximum_rounding_error = resolution / 2.0L,
            .minimum_endpoint_exact = true,
            .maximum_endpoint_exact = true,
            .clipping = linear_quantized.clipping[quantized_row]};
}

auto compute_fixed_encoding(AnalysisContext& context, lispb::schema::TypeId type) -> FixedEncoding {
    auto const& types{context.types};

    auto const& fixed{types.definition(type)};
    auto const& fixed_points{types.definitions().fixed_points};
    auto const fixed_row{fixed_points.index(fixed)};
    auto const sign_bits{fixed_points.signedness[fixed_row] ? std::uint32_t{1} : std::uint32_t{0}};
    auto const whole_bits{fixed_points.total_bits[fixed_row] -
                          fixed_points.fractional_bits[fixed_row] - sign_bits};
    auto const resolution{
        std::ldexp(1.0L, -static_cast<int>(fixed_points.fractional_bits[fixed_row]))};
    auto const scale{std::ldexp(1.0L, static_cast<int>(fixed_points.fractional_bits[fixed_row]))};

    PackedIntegerValue minimum_raw;
    PackedIntegerValue maximum_raw;
    long double minimum_value{};
    long double maximum_value{};
    if (fixed_points.signedness[fixed_row]) {
        auto const negative_magnitude{std::uint64_t{1} << (fixed_points.total_bits[fixed_row] - 1)};
        minimum_raw = PackedIntegerValue::from_parts(true, negative_magnitude);
        maximum_raw = PackedIntegerValue{negative_magnitude - 1};
        auto const range_limit{
            std::ldexp(1.0L,
                       static_cast<int>(fixed_points.total_bits[fixed_row] - 1) -
                           static_cast<int>(fixed_points.fractional_bits[fixed_row]))};
        minimum_value = -range_limit;
        maximum_value = range_limit - resolution;
    } else {
        minimum_raw = PackedIntegerValue{0};
        maximum_raw =
            PackedIntegerValue{fixed_points.total_bits[fixed_row] == 64
                                   ? maximum_unsigned_code
                                   : (std::uint64_t{1} << fixed_points.total_bits[fixed_row]) - 1};
        maximum_value = std::ldexp(1.0L,
                                   static_cast<int>(fixed_points.total_bits[fixed_row]) -
                                       static_cast<int>(fixed_points.fractional_bits[fixed_row])) -
                        resolution;
    }

    auto const minimum_allowed_raw{fixed_points.minimum_raw_value[fixed_row].value_or(minimum_raw)};
    auto const maximum_allowed_raw{fixed_points.maximum_raw_value[fixed_row].value_or(maximum_raw)};
    auto const to_value = [&](PackedIntegerValue const raw) {
        auto const value{static_cast<long double>(raw.magnitude) / scale};
        return raw.negative ? -value : value;
    };
    FixedEncoding result{.signedness = fixed_points.signedness[fixed_row] != 0,
                         .total_bits = fixed_points.total_bits[fixed_row],
                         .fractional_bits = fixed_points.fractional_bits[fixed_row],
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
                             fixed_points.rounding[fixed_row] ==
                                     lispb::codegen::FixedPointRounding::nearest_even
                                 ? resolution / 2.0L
                                 : resolution,
                         .rounding = fixed_points.rounding[fixed_row]};
    return result;
}

auto compute_float_encoding(AnalysisContext& context, lispb::schema::TypeId type) -> FloatEncoding {
    auto const& types{context.types};

    auto const& mini{types.definition(type)};
    auto const& mini_floats{types.definitions().mini_floats};
    auto const mini_row{mini_floats.index(mini)};
    auto const total_bits{mini_floats.sign_bits[mini_row] + mini_floats.exponent_bits[mini_row] +
                          mini_floats.significand_bits[mini_row]};
    auto const exponent_code_count{std::uint64_t{1} << mini_floats.exponent_bits[mini_row]};
    auto const normal_exponent_code_count{exponent_code_count - 2};
    auto const sign_code_count{mini_floats.sign_bits[mini_row] == 0 ? std::uint64_t{1}
                                                                    : std::uint64_t{2}};
    auto const significand_code_count{std::uint64_t{1} << mini_floats.significand_bits[mini_row]};
    auto const minimum_normal_exponent{std::int32_t{1} - mini_floats.exponent_bias[mini_row]};
    auto const maximum_normal_exponent{static_cast<std::int32_t>(exponent_code_count - 2) -
                                       mini_floats.exponent_bias[mini_row]};

    FloatEncoding result{
        .sign_bits = mini_floats.sign_bits[mini_row],
        .exponent_bits = mini_floats.exponent_bits[mini_row],
        .significand_bits = mini_floats.significand_bits[mini_row],
        .total_bits = total_bits,
        .exponent_bias = mini_floats.exponent_bias[mini_row],
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
        .maximum_relative_rounding_error =
            std::ldexp(1.0L, -static_cast<int>(mini_floats.significand_bits[mini_row]) - 1)};

    auto retain_positive_finite =
        [&](long double const value,
            std::string_view const description) -> std::optional<long double> {
        if (std::isfinite(value) && value > 0.0L) {
            return value;
        }
        context.report(
            {DiagnosticSeverity::warning,
             std::string{description} +
                 " is outside the host long-double numerical range; exponent facts remain exact."});
        return std::nullopt;
    };

    result.minimum_positive_normal = retain_positive_finite(
        std::ldexp(1.0L, minimum_normal_exponent), "Minimum positive normal value");
    if (mini_floats.significand_bits[mini_row] != 0) {
        result.minimum_positive_subnormal = retain_positive_finite(
            std::ldexp(1.0L,
                       minimum_normal_exponent -
                           static_cast<int>(mini_floats.significand_bits[mini_row])),
            "Minimum positive subnormal value");
    }
    auto const maximum_significand{
        2.0L - std::ldexp(1.0L, -static_cast<int>(mini_floats.significand_bits[mini_row]))};
    result.maximum_finite = retain_positive_finite(
        std::ldexp(maximum_significand, maximum_normal_exponent), "Maximum finite value");
    if (result.maximum_finite.has_value()) {
        result.minimum_finite = mini_floats.sign_bits[mini_row] == 0
                                  ? std::optional<long double>{0.0L}
                                  : std::optional{-*result.maximum_finite};
    }
    if (mini_floats.exponent_bias[mini_row] >= 1 &&
        mini_floats.exponent_bias[mini_row] <= static_cast<std::int32_t>(exponent_code_count - 2)) {
        result.unit_interval_resolution =
            std::ldexp(1.0L, -static_cast<int>(mini_floats.significand_bits[mini_row]));
    } else {
        context.report(
            {DiagnosticSeverity::warning,
             "1.0 is outside the normal exponent range, so unit-interval resolution is unknown."});
    }
    return result;
}

auto compute_varint_encoding(AnalysisContext& context, lispb::schema::TypeId type)
    -> VarintEncoding {
    auto const& types{context.types};

    auto const& varint{types.definition(type)};
    auto const& integer_varints{types.definitions().integer_varints};
    auto const varint_row{integer_varints.index(varint)};
    auto const source{*lispb::schema::integer_domain(
        types,
        types.occurrences().type[types.occurrences().index(integer_varints.source[varint_row])])};
    auto const& integer_scalars{types.definitions().integer_scalars};
    auto const source_row{integer_scalars.index(source)};

    auto const minimum_endpoint_bytes{integer_varint_bytes(
        integer_scalars.minimum_value[source_row], integer_varints.encoding[varint_row])};
    auto const maximum_endpoint_bytes{integer_varint_bytes(
        integer_scalars.maximum_value[source_row], integer_varints.encoding[varint_row])};
    auto minimum_bytes{std::min(minimum_endpoint_bytes, maximum_endpoint_bytes)};
    auto const zero{PackedIntegerValue{0}};
    if (lispb::codegen::packed_integer_less_equal(integer_scalars.minimum_value[source_row],
                                                  zero) &&
        lispb::codegen::packed_integer_less_equal(zero,
                                                  integer_scalars.maximum_value[source_row])) {
        minimum_bytes = 1;
    }
    auto maximum_bytes{std::max(minimum_endpoint_bytes, maximum_endpoint_bytes)};
    for (auto const code_id : types.declarations().items(integer_scalars.named_codes[source_row])) {
        auto const code{code_id};
        auto const bytes{integer_varint_bytes(
            types.declarations()
                .packed_named_codes.value[types.declarations().packed_named_codes.index(code)],
            integer_varints.encoding[varint_row])};
        minimum_bytes = std::min(minimum_bytes, bytes);
        maximum_bytes = std::max(maximum_bytes, bytes);
    }

    VarintEncoding result{
        .source_type =
            types.occurrences().type[types.occurrences().index(integer_varints.source[varint_row])],
        .encoding = integer_varints.encoding[varint_row],
        .minimum_encoded_bytes = minimum_bytes,
        .maximum_encoded_bytes = maximum_bytes};
    return result;
}

auto compute_enum_domain(AnalysisContext& context, lispb::schema::TypeId type) -> EnumDomain {
    auto const& types{context.types};
    auto const enumeration{types.definition(type)};
    auto const& enums{types.definitions().enums};
    auto const row{enums.index(enumeration)};
    EnumDomain result;
    result.declared_bit_width = enums.bit_width[row];
    result.declared_signedness =
        enums.signedness[row] == lispb::schema::EnumSignedness::unspecified
            ? std::nullopt
            : std::optional{enums.signedness[row] == lispb::schema::EnumSignedness::signed_};
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
    result.codes = {context.storage.codes.num(), value_count};
    for (std::size_t index{}; index < value_count; ++index) {
        auto const enumerator{domains.values(domain)[index]};
        auto const& enumerators{types.declarations().enumerators};
        auto const row{enumerators.index(enumerator)};
        auto& codes{context.storage.codes};
        auto const code_row{codes.append()};
        codes.name()[code_row] = types.strings().view(enumerators.name[row]);
        codes.sentinel()[code_row] = enumerators.sentinel[row];
        codes.count_sentinel()[code_row] = enumerator == domains.count_value[domain_row];
        codes.code()[code_row] = domains.code(domain, index);
    }
    auto const issue_count{domains.issue_count(domain)};
    for (std::uint32_t issue{}; issue < issue_count; ++issue) {
        context.report({.severity = domains.issue_severity(domain, issue) ==
                                            lispb::schema::EnumDomainIssueSeverity::error
                                      ? DiagnosticSeverity::error
                                      : DiagnosticSeverity::warning,
                        .message = domains.issue_message(domain, issue)});
    }

    return result;
}

void analyze_scalar_domains(AnalysisContext& context) {
    auto& scalar{context.storage.scalars};
    for (auto const type : context.types.types()) {
        scalar.append(type);
    }
    for (auto const type : context.types.types()) {
        auto const row{scalar.find(type)};
        if (lispb::schema::integer_domain(context.types, type).has_value()) {
            scalar.integer_domain()[row] = compute_integer_domain(context, type);
        } else if (context.types.definition(type).kind() ==
                   lispb::schema::TypeDefinitionKind::enumeration) {
            scalar.enum_domain()[row] = compute_enum_domain(context, type);
        }
    }
    for (auto const type : context.types.types()) {
        auto const row{scalar.find(type)};
        switch (context.types.definition(type).kind()) {
            case lispb::schema::TypeDefinitionKind::optional_sentinel:
                scalar.optional_sentinel_encoding()[row] =
                    compute_optional_sentinel_encoding(context, type);
                break;
            case lispb::schema::TypeDefinitionKind::optional_presence_bit:
                scalar.optional_presence_encoding()[row] =
                    compute_optional_presence_encoding(context, type);
                break;
            case lispb::schema::TypeDefinitionKind::linear_quantized:
                scalar.quantized_encoding()[row] = compute_quantized_encoding(context, type);
                break;
            case lispb::schema::TypeDefinitionKind::fixed_point:
                scalar.fixed_encoding()[row] = compute_fixed_encoding(context, type);
                break;
            case lispb::schema::TypeDefinitionKind::mini_float:
                scalar.float_encoding()[row] = compute_float_encoding(context, type);
                break;
            case lispb::schema::TypeDefinitionKind::integer_varint:
                scalar.varint_encoding()[row] = compute_varint_encoding(context, type);
                break;
            default:
                break;
        }
    }
}
}
