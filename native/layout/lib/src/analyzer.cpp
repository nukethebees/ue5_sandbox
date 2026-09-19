#include <ioj/layout/analyzer.hpp>

#include <codegen/schema.h>

#include <limits>
#include <utility>

namespace ioj::layout {
namespace {

constexpr std::uint64_t cache_line_bytes{64};

auto checked_add(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t> {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        return std::nullopt;
    }
    return left + right;
}

auto checked_multiply(std::uint64_t const left, std::uint64_t const right)
    -> std::optional<std::uint64_t> {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) {
        return std::nullopt;
    }
    return left * right;
}

auto maximum_unsigned_value(std::uint32_t const bits) -> std::optional<std::uint64_t> {
    if (bits > 64) {
        return std::nullopt;
    }
    if (bits == 64) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return (std::uint64_t{1} << bits) - 1;
}

auto effective_field_width(lispb::schema::TypeId const type,
                           lispb::schema::PackedField const& field,
                           Variant const& variant) -> std::uint32_t {
    auto const found{variant.overrides.packed_field_widths.find(
        FieldOverrideId{.type = type, .field_name = field.name})};
    return found == variant.overrides.packed_field_widths.end() ? field.bit_width : found->second;
}

auto effective_storage_type(lispb::schema::TypeGraph const& types,
                            lispb::schema::TypeId const type,
                            lispb::schema::PackedType const& packed,
                            Variant const& variant) -> std::string {
    auto const found{variant.overrides.packed_storage_types.find(type)};
    if (found != variant.overrides.packed_storage_types.end()) {
        return found->second;
    }
    return physical_type_spelling(types, packed.storage_type.type)
        .value_or(packed.storage_type.cpp_type.spelling);
}

auto effective_column_type(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           lispb::schema::SoaColumn const& column,
                           Variant const& variant) -> std::string {
    auto const found{variant.overrides.soa_column_types.find(
        FieldOverrideId{.type = type, .field_name = column.name})};
    if (found != variant.overrides.soa_column_types.end()) {
        return found->second;
    }
    return physical_type_spelling(types, column.semantic_type.type)
        .value_or(column.semantic_type.cpp_type.spelling);
}

auto minimum_cache_lines(std::uint64_t const bytes) -> std::uint64_t {
    return bytes / cache_line_bytes + (bytes % cache_line_bytes == 0 ? 0 : 1);
}

auto cache_line_tiling(std::uint64_t const element_bytes) -> CacheLineTiling {
    auto const complete_elements{
        element_bytes <= cache_line_bytes ? cache_line_bytes / element_bytes : std::uint64_t{0}};
    return {.cache_line_bytes = cache_line_bytes,
            .element_bytes = element_bytes,
            .exact_elements_per_cache_line =
                element_bytes <= cache_line_bytes && cache_line_bytes % element_bytes == 0
                    ? std::optional<std::uint64_t>{complete_elements}
                    : std::nullopt,
            .complete_elements_from_line_start = complete_elements,
            .boundary_fragment_bytes =
                element_bytes <= cache_line_bytes ? cache_line_bytes % element_bytes : 0,
            .minimum_cache_lines_per_element = minimum_cache_lines(element_bytes)};
}

} // namespace

auto physical_type_spelling(lispb::schema::TypeGraph const& types, lispb::schema::TypeId const type)
    -> std::optional<std::string> {
    std::vector<lispb::schema::TypeId> visited;
    auto current{type};
    while (current.valid() && std::ranges::find(visited, current) == visited.end()) {
        visited.push_back(current);
        auto const& node{types.type(current)};
        if (auto const* external{std::get_if<lispb::schema::ExternalType>(&node.definition)}) {
            return codegen::native_spelling(external->cpp_type.spelling);
        }
        if (auto const* enumeration{std::get_if<lispb::schema::EnumType>(&node.definition)}) {
            current = enumeration->underlying_type.type;
            continue;
        }
        if (auto const* packed{std::get_if<lispb::schema::PackedType>(&node.definition)}) {
            current = packed->storage_type.type;
            continue;
        }
        return std::nullopt;
    }
    return std::nullopt;
}

auto numeric_delta(std::optional<std::uint64_t> const baseline,
                   std::optional<std::uint64_t> const variant) -> std::optional<NumericDelta> {
    if (!baseline.has_value() || !variant.has_value()) {
        return std::nullopt;
    }
    if (*baseline == *variant) {
        return NumericDelta{.direction = NumericDeltaDirection::unchanged,
                            .magnitude = 0,
                            .percentage =
                                *baseline == 0 ? std::nullopt : std::optional<double>{0.0}};
    }
    auto const increased{*variant > *baseline};
    auto const magnitude{increased ? *variant - *baseline : *baseline - *variant};
    return NumericDelta{.direction = increased ? NumericDeltaDirection::increased
                                               : NumericDeltaDirection::decreased,
                        .magnitude = magnitude,
                        .percentage = *baseline == 0 ? std::nullopt
                                                     : std::optional<double>{
                                                           static_cast<double>(magnitude) * 100.0 /
                                                           static_cast<double>(*baseline)}};
}

auto Analyzer::analyze_packed(lispb::schema::TypeGraph const& types,
                              lispb::schema::TypeId const type,
                              Variant const& variant,
                              AbiProfile const& abi) -> PackedAnalysis {
    auto const& packed{std::get<lispb::schema::PackedType>(types.type(type).definition)};
    auto const schema_storage{physical_type_spelling(types, packed.storage_type.type)
                                  .value_or(packed.storage_type.cpp_type.spelling)};
    PackedAnalysis result{.type = type,
                          .schema_storage_type = schema_storage,
                          .storage_type = effective_storage_type(types, type, packed, variant),
                          .storage_overridden =
                              variant.overrides.packed_storage_types.contains(type),
                          .storage_facts = std::nullopt,
                          .storage_bits = std::nullopt,
                          .bits_used = std::nullopt,
                          .unused_bits = std::nullopt,
                          .overflow_bits = std::nullopt,
                          .invalid_raw_value = packed.invalid_raw_value,
                          .fields = {},
                          .diagnostics = {}};
    result.storage_facts = abi.find(result.storage_type);
    if (!result.storage_facts.has_value()) {
        result.diagnostics.push_back(
            {DiagnosticSeverity::error,
             "Unknown physical facts for packed storage type '" + result.storage_type + "'."});
    } else {
        result.storage_bits = checked_multiply(result.storage_facts->size_bytes, 8);
        if (!result.storage_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Packed storage bit count overflows uint64."});
        }
        if (!result.storage_facts->unsigned_value_bits.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::warning,
                 "Packed storage type '" + result.storage_type +
                     "' is not described as an unsigned integer by this ABI profile."});
        }
    }

    std::uint64_t offset{};
    bool width_overflow{};
    result.fields.reserve(packed.fields.size());
    for (auto const& field : packed.fields) {
        auto const width{effective_field_width(type, field, variant)};
        auto const next_offset{checked_add(offset, width)};
        PackedFieldAnalysis field_result{
            .name = field.name,
            .semantic_type = field.semantic_type.type,
            .logical_type = types.type(field.semantic_type.type).cpp_spelling,
            .kind = field.kind,
            .schema_bit_width = field.bit_width,
            .bit_width = width,
            .overridden = variant.overrides.packed_field_widths.contains(
                FieldOverrideId{.type = type, .field_name = field.name}),
            .least_significant_bit = offset,
            .most_significant_bit = std::nullopt,
            .maximum_unsigned_value = maximum_unsigned_value(width)};
        if (width == 0) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed field '" + field.name + "' must use at least one bit."});
        }
        if (!field_result.maximum_unsigned_value.has_value()) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Packed field '" + field.name + "' exceeds the 64-bit V1 range limit."});
        }
        if (next_offset.has_value()) {
            if (width != 0) {
                field_result.most_significant_bit = *next_offset - 1;
            }
            offset = *next_offset;
        } else {
            width_overflow = true;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "Packed field bit offsets overflow uint64."});
        }
        result.fields.push_back(std::move(field_result));
    }

    if (!width_overflow) {
        result.bits_used = offset;
        if (result.storage_bits.has_value()) {
            if (*result.bits_used > *result.storage_bits) {
                result.overflow_bits = *result.bits_used - *result.storage_bits;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error,
                     "Packed fields use more bits than the selected storage type provides."});
            } else {
                result.unused_bits = *result.storage_bits - *result.bits_used;
            }
        }
    }

    if (packed.invalid_raw_value.has_value() && result.storage_bits.has_value() &&
        *result.storage_bits < 64) {
        auto const storage_max{
            maximum_unsigned_value(static_cast<std::uint32_t>(*result.storage_bits))};
        if (storage_max.has_value() && *packed.invalid_raw_value > *storage_max) {
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "The invalid raw value does not fit the selected packed storage type."});
        }
    }
    return result;
}

auto Analyzer::analyze_soa(lispb::schema::TypeGraph const& types,
                           lispb::schema::TypeId const type,
                           Variant const& variant,
                           AbiProfile const& abi,
                           std::uint64_t const default_capacity) -> SoaAnalysis {
    auto const& soa{std::get<lispb::schema::SoaType>(types.type(type).definition)};
    auto capacity{default_capacity};
    bool capacity_overridden{};
    if (auto const found{variant.overrides.capacities.find(type)};
        found != variant.overrides.capacities.end()) {
        capacity = found->second;
        capacity_overridden = true;
    }
    SoaAnalysis result{.type = type,
                       .capacity = capacity,
                       .capacity_overridden = capacity_overridden,
                       .columns = {},
                       .bytes_per_logical_element = std::nullopt,
                       .total_payload_bytes = std::nullopt,
                       .diagnostics = {}};
    std::uint64_t row_bytes{};
    std::uint64_t total_bytes{};
    bool complete{true};
    result.columns.reserve(soa.columns.size());

    for (auto const& column : soa.columns) {
        SoaColumnAnalysis column_result{
            .name = column.name,
            .semantic_type = column.semantic_type.type,
            .schema_type = types.type(column.semantic_type.type).cpp_spelling,
            .physical_type = effective_column_type(types, type, column, variant),
            .overridden = variant.overrides.soa_column_types.contains(
                FieldOverrideId{.type = type, .field_name = column.name}),
            .type_facts = std::nullopt,
            .total_bytes = std::nullopt,
            .minimum_cache_lines = std::nullopt,
            .elements_per_cache_line = std::nullopt,
            .cache_line_tiling = std::nullopt};
        column_result.type_facts = abi.find(column_result.physical_type);
        if (!column_result.type_facts.has_value()) {
            complete = false;
            result.diagnostics.push_back({DiagnosticSeverity::error,
                                          "Unknown physical facts for SoA column '" + column.name +
                                              "' type '" + column_result.physical_type + "'."});
            result.columns.push_back(std::move(column_result));
            continue;
        }

        auto const size{column_result.type_facts->size_bytes};
        if (size == 0) {
            complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "SoA column '" + column.name + "' has a zero-byte physical type."});
            result.columns.push_back(std::move(column_result));
            continue;
        }
        column_result.total_bytes = checked_multiply(size, capacity);
        if (!column_result.total_bytes.has_value()) {
            complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error,
                 "Byte count overflows uint64 for SoA column '" + column.name + "'."});
        } else {
            column_result.minimum_cache_lines = minimum_cache_lines(*column_result.total_bytes);
        }
        column_result.cache_line_tiling = cache_line_tiling(size);
        column_result.elements_per_cache_line =
            column_result.cache_line_tiling->exact_elements_per_cache_line;

        auto const next_row_bytes{checked_add(row_bytes, size)};
        if (!next_row_bytes.has_value()) {
            complete = false;
            result.diagnostics.push_back(
                {DiagnosticSeverity::error, "SoA bytes per logical element overflow uint64."});
        } else {
            row_bytes = *next_row_bytes;
        }
        if (column_result.total_bytes.has_value()) {
            auto const next_total{checked_add(total_bytes, *column_result.total_bytes)};
            if (!next_total.has_value()) {
                complete = false;
                result.diagnostics.push_back(
                    {DiagnosticSeverity::error, "SoA total payload bytes overflow uint64."});
            } else {
                total_bytes = *next_total;
            }
        }
        result.columns.push_back(std::move(column_result));
    }

    if (complete) {
        result.bytes_per_logical_element = row_bytes;
        result.total_payload_bytes = total_bytes;
    }
    return result;
}

} // namespace ioj::layout
