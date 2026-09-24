#include "lowering.h"
#include "lowering_utils.h"
#include "packed_value_internal.h"

#include <lispb/schema/enum_domain.h>

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>

namespace codegen::detail {
namespace {

auto hex_value(std::uint64_t const value) -> std::string {
    std::ostringstream output;
    output << "0x" << std::hex << value;
    return output.str();
}

auto integer_cast_literal(std::string const& type, PackedIntegerValue const value) -> std::string {
    if (value.negative && value.magnitude == (std::uint64_t{1} << 63)) {
        return "std::numeric_limits<" + type + ">::min()";
    }
    auto magnitude{std::to_string(value.magnitude)};
    if (!value.negative &&
        value.magnitude > static_cast<std::uint64_t>((std::numeric_limits<std::int64_t>::max)())) {
        magnitude += "ULL";
    }
    return "static_cast<" + type + ">(" + (value.negative ? "-" : "") + magnitude + ")";
}

auto dependency_for_integer(CppType const& type) -> std::optional<TypeDependency> {
    if (!type.dependencies.empty()) {
        return std::nullopt;
    }
    if (type.spelling.starts_with("std::uint") || type.spelling.starts_with("std::int")) {
        return TypeDependency{type.spelling, "cstdint", {}};
    }
    if (type.spelling.starts_with("uint") || type.spelling.starts_with("int")) {
        return TypeDependency{type.spelling, "CoreTypes.h", {}};
    }
    return std::nullopt;
}

auto semantic_field_for(lispb::schema::PackedType const& packed, std::string const& field_name)
    -> lispb::schema::PackedField const& {
    for (auto const& segment : packed.segments) {
        if (auto const* field{std::get_if<lispb::schema::PackedField>(&segment)};
            field != nullptr && field->name == field_name) {
            return *field;
        }
    }
    throw std::invalid_argument{"Missing semantic packed field '" + field_name + "'"};
}

auto enum_type_for_field(lispb::schema::PackedType const& packed,
                         lispb::schema::TypeGraph const& type_graph,
                         std::string const& field_name) -> lispb::schema::EnumType const* {
    auto const& field{semantic_field_for(packed, field_name)};
    return std::get_if<lispb::schema::EnumType>(
        &type_graph.type(field.semantic_type.type).definition);
}

auto linear_quantized_type_for_field(lispb::schema::PackedType const& packed,
                                     lispb::schema::TypeGraph const& type_graph,
                                     std::string const& field_name)
    -> lispb::schema::LinearQuantizedType const* {
    auto const& field{semantic_field_for(packed, field_name)};
    return std::get_if<lispb::schema::LinearQuantizedType>(
        &type_graph.type(field.semantic_type.type).definition);
}

auto fixed_point_type_for_field(lispb::schema::PackedType const& packed,
                                lispb::schema::TypeGraph const& type_graph,
                                std::string const& field_name)
    -> lispb::schema::FixedPointType const* {
    auto const& field{semantic_field_for(packed, field_name)};
    return std::get_if<lispb::schema::FixedPointType>(
        &type_graph.type(field.semantic_type.type).definition);
}

auto mini_float_type_for_field(lispb::schema::PackedType const& packed,
                               lispb::schema::TypeGraph const& type_graph,
                               std::string const& field_name)
    -> lispb::schema::MiniFloatType const* {
    auto const& field{semantic_field_for(packed, field_name)};
    return std::get_if<lispb::schema::MiniFloatType>(
        &type_graph.type(field.semantic_type.type).definition);
}

auto enum_domain(lispb::schema::EnumType const& type) -> lispb::schema::EnumDomain {
    std::vector<lispb::schema::EnumDomainInput> values;
    values.reserve(type.enumerators.size());
    for (auto const& enumerator : type.enumerators) {
        values.push_back({.name = enumerator.name,
                          .explicit_value = enumerator.explicit_value,
                          .reserved = enumerator.count_sentinel ||
                                      (type.count.has_value() && enumerator.name == *type.count)});
    }
    return lispb::schema::analyze_enum_domain(values, type.signedness);
}

auto resolved_width_for_field(lispb::schema::PackedType const& packed,
                              std::string const& field_name) -> std::uint32_t {
    return semantic_field_for(packed, field_name).bit_width;
}

auto fixed_width_integer_spelling(bool const signedness, std::uint32_t const required_bits)
    -> std::string {
    auto const storage_bits{required_bits <= 8    ? 8
                            : required_bits <= 16 ? 16
                            : required_bits <= 32 ? 32
                                                  : 64};
    return "std::" + std::string{signedness ? "int" : "uint"} + std::to_string(storage_bits) + "_t";
}

void lower_semantic_fields(PackedValueSchema& schema,
                           lispb::schema::PackedType const& packed,
                           lispb::schema::TypeGraph const& type_graph) {
    for (auto& segment : schema.segments) {
        auto* field{std::get_if<PackedFieldSchema>(&segment)};
        if (field == nullptr) {
            continue;
        }

        auto const& semantic_field{semantic_field_for(packed, field->name)};
        auto const* scalar{lispb::schema::packed_integer_domain(
            type_graph.type(semantic_field.semantic_type.type), *field)};
        if (scalar != nullptr) {
            field->type = TypeRef{
                .name = fixed_width_integer_spelling(scalar->signedness, semantic_field.bit_width)};
            field->minimum_value = semantic_field.minimum_value;
            field->maximum_value = semantic_field.maximum_value;
            field->named_codes.clear();
            field->named_codes.reserve(semantic_field.named_codes.size());
            for (auto const& code : semantic_field.named_codes) {
                field->named_codes.push_back(
                    {.name = code.name, .value = code.value, .sentinel = code.sentinel});
            }
            continue;
        }

        auto const* quantized{std::get_if<lispb::schema::LinearQuantizedType>(
            &type_graph.type(semantic_field.semantic_type.type).definition)};
        if (quantized != nullptr) {
            field->type =
                TypeRef{.name = fixed_width_integer_spelling(false, semantic_field.bit_width)};
            continue;
        }

        auto const* fixed{std::get_if<lispb::schema::FixedPointType>(
            &type_graph.type(semantic_field.semantic_type.type).definition)};
        if (fixed != nullptr) {
            field->type = TypeRef{
                .name = fixed_width_integer_spelling(fixed->signedness, semantic_field.bit_width)};
            continue;
        }

        if (std::holds_alternative<lispb::schema::MiniFloatType>(
                type_graph.type(semantic_field.semantic_type.type).definition)) {
            field->type =
                TypeRef{.name = fixed_width_integer_spelling(false, semantic_field.bit_width)};
        }
    }
}

auto packed_field_type_alias(PackedFieldSchema const& field) -> std::string {
    if (field.kind == PackedFieldKind::linear_quantized ||
        field.kind == PackedFieldKind::mini_float) {
        return field.name + "_encoded_type";
    }
    if (field.kind == PackedFieldKind::fixed_point) {
        return field.name + "_raw_type";
    }
    return field.name + "_type";
}

auto packed_field_accessor(PackedFieldSchema const& field) -> std::string {
    if (field.kind == PackedFieldKind::linear_quantized ||
        field.kind == PackedFieldKind::mini_float) {
        return field.name + "_encoded";
    }
    if (field.kind == PackedFieldKind::fixed_point) {
        return field.name + "_raw";
    }
    return field.name;
}

auto packed_field_value_name(PackedFieldSchema const& field) -> std::string {
    return packed_field_accessor(field) + "_value";
}

auto maximum_quantized_code(lispb::schema::LinearQuantizedType const& quantized) -> std::uint64_t {
    auto const all_codes{quantized.bit_width == 64 ? (std::numeric_limits<std::uint64_t>::max)()
                                                   : (std::uint64_t{1} << quantized.bit_width) - 1};
    return all_codes - quantized.reserved_codes;
}

struct PackedFieldLayout {
    PackedFieldSchema const& field;
    CppType type;
    lispb::schema::EnumType const* enum_type;
    lispb::schema::LinearQuantizedType const* linear_quantized_type;
    lispb::schema::FixedPointType const* fixed_point_type;
    lispb::schema::MiniFloatType const* mini_float_type;
    std::uint32_t bits;
    int offset;
    std::uint64_t value_mask;
};

auto packed_field_layouts(PackedValueSchema const& schema,
                          TypeRegistry const& types,
                          lispb::schema::PackedType const& packed,
                          lispb::schema::TypeGraph const& type_graph,
                          int const storage_bits) -> std::vector<PackedFieldLayout> {
    auto const all_bits{storage_bits == 64 ? std::numeric_limits<std::uint64_t>::max()
                                           : (std::uint64_t{1} << storage_bits) - 1};
    std::vector<PackedFieldLayout> layouts;
    layouts.reserve(schema.segments.size());

    auto const most_significant_first{packed.bit_order ==
                                      codegen::PackedBitOrder::most_significant_first};
    auto offset{most_significant_first ? storage_bits : 0};
    for (auto const& segment : schema.segments) {
        auto const* field{std::get_if<PackedFieldSchema>(&segment)};
        auto const bits{field != nullptr ? resolved_width_for_field(packed, field->name)
                                         : static_cast<std::uint32_t>(
                                               std::get<PackedReservedBitsSchema>(segment).bits)};
        if (most_significant_first) {
            offset -= static_cast<int>(bits);
        }
        if (field == nullptr) {
            if (!most_significant_first) {
                offset += static_cast<int>(bits);
            }
            continue;
        }

        auto const value_mask{bits == 64 ? all_bits : (std::uint64_t{1} << bits) - 1};
        layouts.push_back(PackedFieldLayout{
            .field = *field,
            .type = resolve_type(field->type, types),
            .enum_type = enum_type_for_field(packed, type_graph, field->name),
            .linear_quantized_type =
                linear_quantized_type_for_field(packed, type_graph, field->name),
            .fixed_point_type = fixed_point_type_for_field(packed, type_graph, field->name),
            .mini_float_type = mini_float_type_for_field(packed, type_graph, field->name),
            .bits = bits,
            .offset = offset,
            .value_mask = value_mask,
        });
        if (!most_significant_first) {
            offset += static_cast<int>(bits);
        }
    }

    return layouts;
}

auto enum_count_value(lispb::schema::EnumType const& type) -> std::optional<std::uint64_t> {
    if (!type.count.has_value()) {
        return std::nullopt;
    }

    auto const domain{enum_domain(type)};
    auto const value{
        std::ranges::find(domain.values, *type.count, &lispb::schema::EnumDomainValue::name)};
    if (value == domain.values.end() || !value->code.has_value() || value->code->negative) {
        return std::nullopt;
    }
    return value->code->magnitude;
}

auto invalid_value_may_be_constructed(std::optional<std::uint64_t> const invalid_value,
                                      std::vector<PackedFieldLayout> const& fields) -> bool {
    if (!invalid_value.has_value()) {
        return false;
    }

    for (auto const& packed_field : fields) {
        auto const encoded_value{(*invalid_value >> packed_field.offset) & packed_field.value_mask};
        if (packed_field.linear_quantized_type != nullptr) {
            if (encoded_value > maximum_quantized_code(*packed_field.linear_quantized_type)) {
                return false;
            }
            continue;
        }
        if (packed_field.enum_type == nullptr) {
            continue;
        }
        auto const count{enum_count_value(*packed_field.enum_type)};
        if (!count.has_value()) {
            continue;
        }
        if (encoded_value >= *count) {
            return false;
        }
    }

    return true;
}

void append_make_parameters(std::string& output, std::vector<PackedFieldLayout> const& fields) {
    for (std::size_t index{}; index < fields.size(); ++index) {
        if (index != 0) {
            output += ", ";
        }
        output +=
            fields[index].type.spelling + " const " + packed_field_value_name(fields[index].field);
    }
}

void append_make_arguments(std::string& output, std::vector<PackedFieldLayout> const& fields) {
    for (std::size_t index{}; index < fields.size(); ++index) {
        if (index != 0) {
            output += ", ";
        }
        output += packed_field_value_name(fields[index].field);
    }
}

void append_semantic_range_assertion(std::string& output,
                                     PackedFieldSchema const& field,
                                     CppType const& type) {
    if (!field.minimum_value.has_value()) {
        return;
    }

    output += "        assert((" + field.name +
              "_value >= " + integer_cast_literal(type.spelling, *field.minimum_value) + " && " +
              field.name +
              "_value <= " + integer_cast_literal(type.spelling, *field.maximum_value) + ")";
    for (auto const& code : field.named_codes) {
        if (code.sentinel) {
            output += " || " + field.name + "_value == " + field.name + "_" + code.name;
        }
    }
    output += ");\n";
}

void append_immutable_validation(std::string& output,
                                 std::vector<PackedFieldLayout> const& fields) {
    for (auto const& packed_field : fields) {
        auto const& field{packed_field.field};
        auto const value_name{packed_field_value_name(field)};
        if (packed_field.type.spelling == "bool") {
            continue;
        }
        if (packed_field.linear_quantized_type != nullptr) {
            output += "        assert(" + value_name + " <= " + field.name + "_maximum_encoded);\n";
            continue;
        }
        if (packed_field.fixed_point_type != nullptr) {
            output += "        assert(" + value_name + " >= " + field.name +
                      "_minimum_allowed_raw && " + value_name + " <= " + field.name +
                      "_maximum_allowed_raw);\n";
            continue;
        }
        if (packed_field.mini_float_type != nullptr) {
            output += "        assert(" + value_name + " <= " + field.name + "_maximum_encoded);\n";
            continue;
        }
        if (field.kind == PackedFieldKind::enumeration) {
            if (packed_field.enum_type != nullptr && packed_field.enum_type->count.has_value()) {
                output += "        assert(" + value_name + " < " + packed_field.type.spelling +
                          "::" + *packed_field.enum_type->count + ");\n";
            } else if (packed_field.enum_type == nullptr) {
                output += "        assert(static_cast<" + field.name + "_underlying_type>(" +
                          value_name + ") <= static_cast<" + field.name + "_underlying_type>(" +
                          field.name + "_value_mask));\n";
            }
            continue;
        }
        if (field.kind == PackedFieldKind::signed_integer) {
            output += "        assert(" + value_name + " >= " + field.name + "_minimum && " +
                      value_name + " <= " + field.name + "_maximum);\n";
        } else {
            output += "        assert(" + value_name + " <= static_cast<" +
                      packed_field.type.spelling + ">(" + field.name + "_value_mask));\n";
        }
        append_semantic_range_assertion(output, field, packed_field.type);
    }
}

auto packed_field_expression(PackedFieldLayout const& packed_field) -> std::string {
    auto const& field{packed_field.field};
    auto const value_name{packed_field_value_name(field)};
    auto expression{std::string{"((static_cast<storage_type>("}};
    if (field.kind == PackedFieldKind::enumeration) {
        expression += "static_cast<" + field.name + "_underlying_type>(" + value_name + ")";
    } else {
        expression += value_name;
    }
    expression += ") & " + field.name + "_value_mask) << " + field.name + "_offset)";
    return expression;
}

auto packed_raw_value_expression(std::vector<PackedFieldLayout> const& fields) -> std::string {
    std::string expression{"static_cast<storage_type>(\n"};
    for (std::size_t index{}; index < fields.size(); ++index) {
        expression += "            " + packed_field_expression(fields[index]);
        expression += index + 1 < fields.size() ? " |\n" : ")";
    }
    return expression;
}

void append_mutable_construction(std::string& output,
                                 PackedValueSchema const& schema,
                                 std::vector<PackedFieldLayout> const& fields) {
    output += "    [[nodiscard]] static constexpr auto try_make(";
    append_make_parameters(output, fields);
    output += ", " + schema.name + "& out_result) noexcept -> bool {\n";
    output += "        " + schema.name + " result{storage_type{0}};\n";
    for (auto const& packed_field : fields) {
        auto const accessor{packed_field_accessor(packed_field.field)};
        output += "        if (!result.try_set_" + accessor + "(" +
                  packed_field_value_name(packed_field.field) + ")) {\n";
        output += "            return false;\n        }\n";
    }
    output += "        if (!result.is_valid()) {\n            return false;\n        }\n";
    output += "        out_result = result;\n        return true;\n    }\n\n";

    output += "    [[nodiscard]] static constexpr auto make(";
    append_make_parameters(output, fields);
    output += ") noexcept -> " + schema.name + " {\n";
    output += "        " + schema.name + " result;\n";
    output += "        [[maybe_unused]] auto const success{try_make(";
    append_make_arguments(output, fields);
    output += ", result)};\n";
    output += "        assert(success && \"Packed field value does not fit.\");\n";
    output += "        return result;\n    }\n\n";
}

void append_immutable_construction(std::string& output,
                                   PackedValueSchema const& schema,
                                   std::vector<PackedFieldLayout> const& fields) {
    output += "    [[nodiscard]] static constexpr auto make(";
    append_make_parameters(output, fields);
    output += ") noexcept -> " + schema.name + " {\n";
    append_immutable_validation(output, fields);

    auto const raw_value{packed_raw_value_expression(fields)};
    if (invalid_value_may_be_constructed(schema.invalid_value, fields)) {
        output += "        auto const raw{" + raw_value + "};\n";
        output += "        assert(raw != invalid_value);\n";
        output += "        return " + schema.name + "{raw};\n    }\n\n";
        return;
    }
    output += "        return " + schema.name + "{" + raw_value + "};\n    }\n\n";
}

auto packed_value_text(PackedValueSchema const& source_schema,
                       TypeRegistry const& types,
                       lispb::schema::PackedType const& packed,
                       lispb::schema::TypeGraph const& type_graph) -> Raw {
    auto schema{source_schema};
    lower_semantic_fields(schema, packed, type_graph);

    auto const storage{resolve_type(schema.storage_type, types)};
    auto const storage_bits{*packed_unsigned_width(storage.spelling)};
    auto const all_bits{storage_bits == 64 ? std::numeric_limits<std::uint64_t>::max()
                                           : (std::uint64_t{1} << storage_bits) - 1};
    std::vector<PackedFieldSchema const*> fields;
    fields.reserve(schema.segments.size());
    for (auto const& segment : schema.segments) {
        if (auto const* field{std::get_if<PackedFieldSchema>(&segment)}) {
            fields.push_back(field);
        }
    }
    auto const field_layouts{packed_field_layouts(schema, types, packed, type_graph, storage_bits)};

    std::vector<TypeDependency> dependencies{
        TypeDependency{"assert", "cassert", {}},
        TypeDependency{"std::strong_ordering", "compare", {}},
        TypeDependency{"std::numeric_limits", "limits", {}},
        TypeDependency{"std::is_enum_v", "type_traits", {}},
    };
    dependencies.insert(
        dependencies.end(), storage.dependencies.begin(), storage.dependencies.end());
    if (auto dependency{dependency_for_integer(storage)}) {
        dependencies.push_back(std::move(*dependency));
    }

    auto output{std::string{"struct "} + schema.export_specifier.value_or("")};
    if (schema.export_specifier.has_value()) {
        output += " ";
    }
    output += schema.name + " {\n";
    output += "    using storage_type = " + storage.spelling + ";\n";
    output += "    static_assert(std::is_unsigned_v<storage_type>);\n";
    output += "    static_assert(std::numeric_limits<storage_type>::digits == " +
              std::to_string(storage_bits) + ");\n";

    auto const most_significant_first{packed.bit_order ==
                                      codegen::PackedBitOrder::most_significant_first};
    auto offset{most_significant_first ? storage_bits : 0};
    for (auto const& segment : schema.segments) {
        auto const* field{std::get_if<PackedFieldSchema>(&segment)};
        auto const segment_bits{
            field != nullptr ? static_cast<int>(resolved_width_for_field(packed, field->name))
                             : std::get<PackedReservedBitsSchema>(segment).bits};
        if (most_significant_first) {
            offset -= segment_bits;
        }
        if (field == nullptr) {
            if (!most_significant_first) {
                offset += segment_bits;
            }
            continue;
        }
        auto const field_bits{static_cast<std::uint32_t>(segment_bits)};
        auto const field_type{resolve_type(field->type, types)};
        auto const* quantized_type{
            linear_quantized_type_for_field(packed, type_graph, field->name)};
        auto const* fixed_type{fixed_point_type_for_field(packed, type_graph, field->name)};
        dependencies.insert(
            dependencies.end(), field_type.dependencies.begin(), field_type.dependencies.end());
        if (auto dependency{dependency_for_integer(field_type)}) {
            dependencies.push_back(std::move(*dependency));
        }
        output +=
            "    using " + packed_field_type_alias(*field) + " = " + field_type.spelling + ";\n";
        if (field->kind == PackedFieldKind::enumeration) {
            output += "    using " + field->name + "_underlying_type = std::underlying_type_t<" +
                      field_type.spelling + ">;\n";
            output += "    static_assert(std::is_enum_v<" + field_type.spelling + ">);\n";
            output +=
                "    static_assert(std::is_unsigned_v<" + field->name + "_underlying_type>);\n";
            output += "    static_assert(std::numeric_limits<" + field->name +
                      "_underlying_type>::digits >= " + std::to_string(field_bits) + ");\n";
        } else if (field->kind == PackedFieldKind::signed_integer) {
            output += "    static_assert(std::is_signed_v<" + field_type.spelling + ">);\n";
            output += "    static_assert(std::numeric_limits<" + field_type.spelling +
                      ">::digits + 1 >= " + std::to_string(field_bits) + ");\n";
        } else if (field->kind == PackedFieldKind::linear_quantized ||
                   field->kind == PackedFieldKind::mini_float) {
            output += "    static_assert(std::is_unsigned_v<" + field_type.spelling + ">);\n";
            output += "    static_assert(std::numeric_limits<" + field_type.spelling +
                      ">::digits >= " + std::to_string(field_bits) + ");\n";
        } else if (field->kind == PackedFieldKind::fixed_point) {
            output += "    static_assert(std::is_" +
                      std::string{fixed_type->signedness ? "signed" : "unsigned"} + "_v<" +
                      field_type.spelling + ">);\n";
            output += "    static_assert(std::numeric_limits<" + field_type.spelling + ">::digits" +
                      (fixed_type->signedness ? " + 1" : "") + " >= " + std::to_string(field_bits) +
                      ");\n";
        }

        auto const value_mask{field_bits == 64 ? all_bits : (std::uint64_t{1} << field_bits) - 1};
        auto const mask{static_cast<std::uint64_t>(value_mask << offset) & all_bits};
        output += "\n    inline static constexpr int " + field->name + "_offset{" +
                  std::to_string(offset) + "};\n";
        output += "    inline static constexpr int " + field->name + "_bits{" +
                  std::to_string(field_bits) + "};\n";
        output += "    inline static constexpr storage_type " + field->name +
                  "_value_mask{storage_type{" + hex_value(value_mask) + "}};\n";
        output += "    inline static constexpr storage_type " + field->name +
                  "_mask{storage_type{" + hex_value(mask) + "}};\n";
        if (quantized_type != nullptr) {
            output += "    inline static constexpr " + packed_field_type_alias(*field) + " " +
                      field->name + "_maximum_encoded{" + packed_field_type_alias(*field) + "{" +
                      hex_value(maximum_quantized_code(*quantized_type)) + "}};\n";
        }
        if (field->kind == PackedFieldKind::mini_float) {
            output += "    inline static constexpr " + packed_field_type_alias(*field) + " " +
                      field->name + "_maximum_encoded{" + packed_field_type_alias(*field) + "{" +
                      hex_value(value_mask) + "}};\n";
        }
        if (fixed_type != nullptr) {
            dependencies.push_back(TypeDependency{"std::isfinite", "cmath", {}});
            auto const raw_type{packed_field_type_alias(*field)};
            if (fixed_type->signedness) {
                auto const native_width{*packed_signed_width(field_type.spelling)};
                if (field_bits == static_cast<std::uint32_t>(native_width)) {
                    output += "    inline static constexpr " + raw_type + " " + field->name +
                              "_minimum_raw{std::numeric_limits<" + raw_type + ">::min()};\n";
                    output += "    inline static constexpr " + raw_type + " " + field->name +
                              "_maximum_raw{std::numeric_limits<" + raw_type + ">::max()};\n";
                } else {
                    auto const sign_magnitude{std::uint64_t{1} << (field_bits - 1)};
                    output += "    inline static constexpr " + raw_type + " " + field->name +
                              "_minimum_raw{static_cast<" + raw_type + ">(-" +
                              std::to_string(sign_magnitude) + ")};\n";
                    output += "    inline static constexpr " + raw_type + " " + field->name +
                              "_maximum_raw{static_cast<" + raw_type + ">(" +
                              std::to_string(sign_magnitude - 1) + ")};\n";
                }
            } else {
                output += "    inline static constexpr " + raw_type + " " + field->name +
                          "_minimum_raw{" + raw_type + "{0}};\n";
                output += "    inline static constexpr " + raw_type + " " + field->name +
                          "_maximum_raw{" + raw_type + "{" + hex_value(value_mask) + "}};\n";
            }
            output += "    inline static constexpr " + raw_type + " " + field->name +
                      "_minimum_allowed_raw{" +
                      (fixed_type->minimum_raw_value.has_value()
                           ? integer_cast_literal(raw_type, *fixed_type->minimum_raw_value)
                           : field->name + "_minimum_raw") +
                      "};\n";
            output += "    inline static constexpr " + raw_type + " " + field->name +
                      "_maximum_allowed_raw{" +
                      (fixed_type->maximum_raw_value.has_value()
                           ? integer_cast_literal(raw_type, *fixed_type->maximum_raw_value)
                           : field->name + "_maximum_raw") +
                      "};\n";
        }
        if (field->kind == PackedFieldKind::signed_integer) {
            auto const signed_width{*packed_signed_width(field_type.spelling)};
            if (field_bits == static_cast<std::uint32_t>(signed_width)) {
                output += "    inline static constexpr " + field_type.spelling + " " + field->name +
                          "_minimum{std::numeric_limits<" + field_type.spelling + ">::min()};\n";
                output += "    inline static constexpr " + field_type.spelling + " " + field->name +
                          "_maximum{std::numeric_limits<" + field_type.spelling + ">::max()};\n";
            } else {
                auto const sign_magnitude{std::uint64_t{1} << (field_bits - 1)};
                output += "    inline static constexpr " + field_type.spelling + " " + field->name +
                          "_minimum{static_cast<" + field_type.spelling + ">(-" +
                          std::to_string(sign_magnitude) + ")};\n";
                output += "    inline static constexpr " + field_type.spelling + " " + field->name +
                          "_maximum{static_cast<" + field_type.spelling + ">(" +
                          std::to_string(sign_magnitude - 1) + ")};\n";
            }
        }
        for (auto const& code : field->named_codes) {
            output += "    inline static constexpr " + field_type.spelling + " " + field->name +
                      "_" + code.name + "{" +
                      integer_cast_literal(field_type.spelling, code.value) + "};\n";
        }

        if (field->kind == PackedFieldKind::enumeration) {
            if (auto const* enum_type{enum_type_for_field(packed, type_graph, field->name)}) {
                auto const domain{enum_domain(*enum_type)};
                auto const has_opaque_value{
                    std::ranges::any_of(domain.values, [](auto const& value) {
                        return !value.reserved && !value.code.has_value();
                    })};
                if (has_opaque_value) {
                    output += "\n    static_assert([]<auto... values>() consteval -> bool {\n";
                    output += "        return ((static_cast<" + field->name +
                              "_underlying_type>(values) <=\n";
                    output += "                 static_cast<" + field->name + "_underlying_type>(" +
                              field->name + "_value_mask)) && ...);\n";
                    output += "    }.template operator()<\n";
                    bool first_value{true};
                    for (auto const& enumerator : enum_type->enumerators) {
                        if (enum_type->count.has_value() && enumerator.name == *enum_type->count) {
                            continue;
                        }
                        if (!first_value) {
                            output += ",\n";
                        }
                        output += "        " + field_type.spelling + "::" + enumerator.name;
                        first_value = false;
                    }
                    output += "\n    >());\n";
                }
            }
        }
        if (!most_significant_first) {
            offset += segment_bits;
        }
    }

    if (schema.invalid_value.has_value()) {
        output += "\n    inline static constexpr storage_type invalid_value{storage_type{" +
                  hex_value(*schema.invalid_value) + "}};\n";
    }

    output += "\n    constexpr " + schema.name + "() noexcept = default;\n";
    output += "    explicit constexpr " + schema.name +
              "(storage_type const raw) noexcept : value_{raw} {}\n\n";
    output += "    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type {\n";
    output += "        return value_;\n    }\n\n";
    if (schema.mutable_value) {
        append_mutable_construction(output, schema, field_layouts);
    } else {
        append_immutable_construction(output, schema, field_layouts);
    }

    output += "    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {\n";
    std::vector<std::string> validity_checks;
    if (schema.invalid_value.has_value()) {
        validity_checks.push_back("value_ != invalid_value");
    }
    for (auto const* field_pointer : fields) {
        auto const& field{*field_pointer};
        if (field.kind == PackedFieldKind::linear_quantized) {
            validity_checks.push_back(packed_field_accessor(field) + "() <= " + field.name +
                                      "_maximum_encoded");
            continue;
        }
        if ((field.kind == PackedFieldKind::unsigned_integer ||
             field.kind == PackedFieldKind::signed_integer) &&
            field.minimum_value.has_value()) {
            auto const field_type{resolve_type(field.type, types)};
            auto check{"(" + field.name +
                       "() >= " + integer_cast_literal(field_type.spelling, *field.minimum_value) +
                       " && " + field.name + "() <= " +
                       integer_cast_literal(field_type.spelling, *field.maximum_value) + ")"};
            for (auto const& code : field.named_codes) {
                if (code.sentinel) {
                    check += " || " + field.name + "() == " + field.name + "_" + code.name;
                }
            }
            validity_checks.push_back("(" + std::move(check) + ")");
            continue;
        }
        if (field.kind != PackedFieldKind::enumeration) {
            if (field.kind == PackedFieldKind::fixed_point) {
                auto const* fixed_type{fixed_point_type_for_field(packed, type_graph, field.name)};
                if (fixed_type->minimum_raw_value.has_value() ||
                    fixed_type->maximum_raw_value.has_value()) {
                    validity_checks.push_back("(" + field.name + "_raw() >= " + field.name +
                                              "_minimum_allowed_raw && " + field.name +
                                              "_raw() <= " + field.name + "_maximum_allowed_raw)");
                }
            }
            continue;
        }
        auto const* enum_type{enum_type_for_field(packed, type_graph, field.name)};
        if (enum_type != nullptr && enum_type->count.has_value()) {
            auto const field_type{resolve_type(field.type, types)};
            validity_checks.push_back(field.name + "() < " + field_type.spelling +
                                      "::" + *enum_type->count);
        }
    }
    output += "        return ";
    for (std::size_t index{}; index < validity_checks.size(); ++index) {
        if (index != 0) {
            output += " && ";
        }
        output += validity_checks[index];
    }
    output += validity_checks.empty() ? "true;\n    }\n\n" : ";\n    }\n\n";
    output += "    [[nodiscard]] constexpr auto operator<=>(" + schema.name +
              " const&) const noexcept = default;\n";

    for (auto const* field_pointer : fields) {
        auto const& field{*field_pointer};
        auto const field_type{resolve_type(field.type, types)};
        auto const field_bits{resolved_width_for_field(packed, field.name)};
        auto const accessor{packed_field_accessor(field)};
        auto const* fixed_type{fixed_point_type_for_field(packed, type_graph, field.name)};
        output += "\n    [[nodiscard]] constexpr auto " + accessor + "() const noexcept -> " +
                  field_type.spelling + " {\n";
        auto const extracted{"static_cast<storage_type>(value_ >> " + field.name + "_offset) & " +
                             field.name + "_value_mask"};
        if (field_type.spelling == "bool") {
            output += "        return (" + extracted + ") != 0;\n";
        } else if (field.kind == PackedFieldKind::enumeration) {
            output += "        auto const encoded{static_cast<" + field.name +
                      "_underlying_type>(" + extracted + ")};\n";
            output += "        return static_cast<" + field_type.spelling + ">(encoded);\n";
        } else if (field.kind == PackedFieldKind::signed_integer ||
                   (fixed_type != nullptr && fixed_type->signedness)) {
            auto const sign_bit{std::uint64_t{1} << (field_bits - 1)};
            output += "        auto const encoded{" + extracted + "};\n";
            output += "        auto const sign_bit{storage_type{" + hex_value(sign_bit) + "}};\n";
            output += "        if ((encoded & sign_bit) == 0) {\n";
            output += "            return static_cast<" + field_type.spelling + ">(encoded);\n";
            output += "        }\n";
            output += "        auto const magnitude{static_cast<storage_type>(\n";
            output += "            static_cast<storage_type>(~encoded & " + field.name +
                      "_value_mask) + storage_type{1})};\n";
            output += "        if (magnitude == sign_bit) {\n";
            output += "            return " + field.name +
                      (fixed_type != nullptr ? "_minimum_raw;\n" : "_minimum;\n");
            output += "        }\n";
            output += "        return static_cast<" + field_type.spelling + ">(\n";
            output += "            -static_cast<" + field_type.spelling + ">(magnitude));\n";
        } else {
            output +=
                "        return static_cast<" + field_type.spelling + ">(" + extracted + ");\n";
        }
        output += "    }\n";

        if (fixed_type != nullptr) {
            auto const raw_type{packed_field_type_alias(field)};
            auto const fractional_bits{std::to_string(fixed_type->fractional_bits)};
            auto const exclusive_limit{
                std::to_string(fixed_type->signedness ? field_bits - 1 : field_bits)};
            output +=
                "\n    [[nodiscard]] auto " + field.name + "_value() const noexcept -> double {\n";
            output += "        return std::ldexp(static_cast<double>(" + field.name + "_raw()), -" +
                      fractional_bits + ");\n    }\n";
            output += "\n    [[nodiscard]] static auto try_encode_" + field.name +
                      "_value(double const value, " + raw_type + "& out_raw) noexcept -> bool {\n";
            output +=
                "        if (!std::isfinite(value)) {\n            return false;\n        }\n";
            output += "        if (value < std::ldexp(static_cast<double>(" + field.name +
                      "_minimum_allowed_raw), -" + fractional_bits + ") ||\n";
            output += "            value > std::ldexp(static_cast<double>(" + field.name +
                      "_maximum_allowed_raw), -" + fractional_bits + ")) {\n";
            output += "            return false;\n        }\n";
            output += "        auto const scaled{std::ldexp(value, " + fractional_bits + ")};\n";
            output +=
                "        if (!std::isfinite(scaled)) {\n            return false;\n        }\n";
            if (fixed_type->rounding == FixedPointRounding::toward_zero) {
                output += "        auto const rounded{std::trunc(scaled)};\n";
            } else {
                output += "        auto const lower{std::floor(scaled)};\n";
                output += "        auto const fraction{scaled - lower};\n";
                output += "        auto const rounded{fraction > 0.5 ||\n";
                output += "                           (fraction == 0.5 && std::fmod(lower, 2.0) != "
                          "0.0)\n";
                output += "                               ? lower + 1.0\n                          "
                          "     : lower};\n";
            }
            output += "        if (rounded >= std::ldexp(1.0, " + exclusive_limit + ")";
            if (fixed_type->signedness) {
                output += " ||\n            rounded < -std::ldexp(1.0, " + exclusive_limit + ")";
            } else {
                output += " || rounded < 0.0";
            }
            output += ") {\n            return false;\n        }\n";
            output += "        auto const raw{static_cast<" + raw_type + ">(rounded)};\n";
            output += "        if (raw < " + field.name + "_minimum_allowed_raw || raw > " +
                      field.name + "_maximum_allowed_raw) {\n";
            output += "            return false;\n        }\n";
            output += "        out_raw = raw;\n        return true;\n    }\n";
        }

        if (schema.mutable_value) {
            output += "\n    [[nodiscard]] constexpr auto try_set_" + accessor + "(" +
                      field_type.spelling + " const value) noexcept -> bool {\n";
            if (field_type.spelling == "bool") {
                output += "        auto const encoded{static_cast<storage_type>(value)};\n";
            } else if (field.kind == PackedFieldKind::linear_quantized) {
                output += "        if (value > " + field.name + "_maximum_encoded) {\n";
                output += "            return false;\n        }\n";
                output += "        auto const encoded{static_cast<storage_type>(value)};\n";
            } else if (field.kind == PackedFieldKind::mini_float) {
                output += "        if (value > " + field.name + "_maximum_encoded) {\n";
                output += "            return false;\n        }\n";
                output += "        auto const encoded{static_cast<storage_type>(value)};\n";
            } else if (field.kind == PackedFieldKind::fixed_point) {
                output += "        if (value < " + field.name + "_minimum_allowed_raw || value > " +
                          field.name + "_maximum_allowed_raw) {\n";
                output += "            return false;\n        }\n";
                output += "        auto const encoded{static_cast<storage_type>(value)};\n";
            } else if (field.kind == PackedFieldKind::enumeration) {
                output += "        auto const underlying{static_cast<" + field.name +
                          "_underlying_type>(value)};\n";
                output += "        if (underlying > static_cast<" + field.name +
                          "_underlying_type>(" + field.name + "_value_mask)) {\n";
                output += "            return false;\n        }\n";
                if (auto const* enum_type{enum_type_for_field(packed, type_graph, field.name)};
                    enum_type != nullptr && enum_type->count.has_value()) {
                    output += "        if (value >= " + field_type.spelling +
                              "::" + *enum_type->count + ") {\n";
                    output += "            return false;\n        }\n";
                }
                output += "        auto const encoded{static_cast<storage_type>(underlying)};\n";
            } else if (field.kind == PackedFieldKind::signed_integer) {
                output += "        if (value < " + field.name + "_minimum || value > " +
                          field.name + "_maximum) {\n";
                output += "            return false;\n        }\n";
                if (field.minimum_value.has_value()) {
                    output += "        if ((value < " +
                              integer_cast_literal(field_type.spelling, *field.minimum_value) +
                              " || value > " +
                              integer_cast_literal(field_type.spelling, *field.maximum_value) + ")";
                    for (auto const& code : field.named_codes) {
                        if (code.sentinel) {
                            output += " && value != " + field.name + "_" + code.name;
                        }
                    }
                    output += ") {\n";
                    output += "            return false;\n        }\n";
                }
                output += "        auto const encoded{static_cast<storage_type>(value)};\n";
            } else {
                output += "        if (value > static_cast<" + field_type.spelling + ">(" +
                          field.name + "_value_mask)) {\n";
                output += "            return false;\n        }\n";
                if (field.minimum_value.has_value()) {
                    output += "        if ((value < " +
                              integer_cast_literal(field_type.spelling, *field.minimum_value) +
                              " || value > " +
                              integer_cast_literal(field_type.spelling, *field.maximum_value) + ")";
                    for (auto const& code : field.named_codes) {
                        if (code.sentinel) {
                            output += " && value != " + field.name + "_" + code.name;
                        }
                    }
                    output += ") {\n";
                    output += "            return false;\n        }\n";
                }
                output += "        auto const encoded{static_cast<storage_type>(value)};\n";
            }
            output += "        auto const cleared{static_cast<storage_type>(\n";
            output +=
                "            value_ & static_cast<storage_type>(~" + field.name + "_mask))};\n";
            output += "        auto const shifted{static_cast<storage_type>(\n";
            output += "            static_cast<storage_type>(encoded & " + field.name +
                      "_value_mask) << " + field.name + "_offset)};\n";
            output += "        value_ = static_cast<storage_type>(cleared | shifted);\n";
            output += "        return true;\n    }\n";
            if (fixed_type != nullptr) {
                output += "\n    [[nodiscard]] auto try_set_" + field.name +
                          "_value(double const value) noexcept -> bool {\n";
                output += "        " + packed_field_type_alias(field) + " raw{};\n";
                output += "        if (!try_encode_" + field.name +
                          "_value(value, raw)) {\n            return false;\n        }\n";
                output += "        return try_set_" + field.name + "_raw(raw);\n    }\n";
            }

            output += "\n    constexpr void set_" + accessor + "(" + field_type.spelling +
                      " const value) noexcept {\n";
            output += "        if (!try_set_" + accessor + "(value)) {\n";
            output += "            assert(false && \"Packed field value does not fit.\");\n";
            output += "        }\n    }\n";
        }

        if (field.range_helper) {
            output += "\n    [[nodiscard]] static constexpr auto " + field.name + "_range_fits(" +
                      field_type.spelling + " const first, " + field_type.spelling +
                      " const count) noexcept -> bool {\n";
            output += "        return count == 0 ||\n";
            if (field.minimum_value.has_value()) {
                output += "               (first >= " +
                          integer_cast_literal(field_type.spelling, *field.minimum_value) +
                          " && first <= " +
                          integer_cast_literal(field_type.spelling, *field.maximum_value) + " &&\n";
                output += "                count - 1 <= " +
                          integer_cast_literal(field_type.spelling, *field.maximum_value) +
                          " - first);\n";
            } else {
                output += "               (first <= " + field.name +
                          "_value_mask && count - 1 <= " + field.name + "_value_mask - first);\n";
            }
            output += "    }\n";
        }
    }

    output += "  private:\n    storage_type value_{";
    output += schema.invalid_value.has_value() ? "invalid_value" : "";
    output += "};\n};\n";
    output += "static_assert(sizeof(" + schema.name + ") == sizeof(" + schema.name +
              "::storage_type));\n";
    output += "static_assert(std::is_trivially_copyable_v<" + schema.name + ">);\n";
    output += "static_assert(std::is_standard_layout_v<" + schema.name + ">);";

    return Raw{std::move(output), std::move(dependencies)};
}

} // namespace

auto lower_packed_value(PackedValueSchema const& schema,
                        TypeRegistry const& types,
                        lispb::schema::TypeGraph const& type_graph,
                        std::string const& module_name) -> DeclarationEmission {
    auto const type_id{type_graph.find_declared(module_name, schema.name)};
    if (!type_id.has_value()) {
        throw std::invalid_argument{"Missing semantic packed type '" + schema.name + "'"};
    }
    auto const& packed{std::get<lispb::schema::PackedType>(type_graph.type(*type_id).definition)};
    return {.header = {packed_value_text(schema, types, packed, type_graph)}};
}

} // namespace codegen::detail
