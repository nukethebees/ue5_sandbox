#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "packed_value_internal.h"

#include "lispb/schema/integer_domain.h"
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/enumerator_schemas.h>
#include <codegen/schema/fixed_point_rounding.h>
#include <codegen/schema/packed_bit_order.h>
#include <codegen/schema/packed_field_kind.h>
#include <codegen/schema/packed_integer_value.h>
#include <codegen/schema/packed_named_code_schemas.h>
#include <codegen/schema/packed_value_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/enum_domains.h>
#include <lispb/schema/enum_types.h>
#include <lispb/schema/fixed_point_types.h>
#include <lispb/schema/indices.h>
#include <lispb/schema/integer_scalar_types.h>
#include <lispb/schema/linear_quantized_types.h>
#include <lispb/schema/mini_float_types.h>
#include <lispb/schema/packed_fields.h>
#include <lispb/schema/packed_reserved_segments.h>
#include <lispb/schema/packed_types.h>
#include <lispb/schema/type_graph.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto hex_value(std::uint64_t const value) -> std::string {
    std::string output{};
    std::format_to(std::back_inserter(output), "0x{:x}", value);
    return output;
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

auto dependency_for_integer(CppIR& ir, CppTypeId type) -> std::optional<TypeDependencyId> {
    if (!ir.types.dependencies(type).empty()) {
        return std::nullopt;
    }
    if (ir.types.spelling(type).starts_with("std::uint") ||
        ir.types.spelling(type).starts_with("std::int")) {
        return ir.types.dependency(ir.types.spelling(type), "cstdint");
    }
    if (ir.types.spelling(type).starts_with("uint") || ir.types.spelling(type).starts_with("int")) {
        return ir.types.dependency(ir.types.spelling(type), "CoreTypes.h");
    }
    return std::nullopt;
}

auto fixed_width_integer_spelling(bool const signedness, std::uint32_t const required_bits)
    -> std::string {
    auto const storage_bits{required_bits <= 8    ? 8
                            : required_bits <= 16 ? 16
                            : required_bits <= 32 ? 32
                                                  : 64};
    return "std::" + std::string{signedness ? "int" : "uint"} + std::to_string(storage_bits) + "_t";
}

auto packed_field_type_alias(LoweringContext& context, schema::PackedSegmentId const& field)
    -> std::string {
    if (context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] ==
            PackedFieldKind::linear_quantized ||
        context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] ==
            PackedFieldKind::mini_float) {
        return context.text(context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
               "_encoded_type";
    }
    if (context.semantic.definitions()
            .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] ==
        PackedFieldKind::fixed_point) {
        return context.text(context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
               "_raw_type";
    }
    return context.text(
               context.semantic.definitions()
                   .packed_fields.name[context.semantic.definitions().packed_fields.index(field)]) +
           "_type";
}

auto packed_field_accessor(LoweringContext& context, schema::PackedSegmentId const& field)
    -> std::string {
    if (context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] ==
            PackedFieldKind::linear_quantized ||
        context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] ==
            PackedFieldKind::mini_float) {
        return context.text(context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
               "_encoded";
    }
    if (context.semantic.definitions()
            .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] ==
        PackedFieldKind::fixed_point) {
        return context.text(context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
               "_raw";
    }
    return context.text(
        context.semantic.definitions()
            .packed_fields.name[context.semantic.definitions().packed_fields.index(field)]);
}

auto packed_field_value_name(LoweringContext& context, schema::PackedSegmentId const& field)
    -> std::string {
    return packed_field_accessor(context, field) + "_value";
}

auto maximum_quantized_code(LoweringContext& context, schema::TypeDefinitionId const quantized)
    -> std::uint64_t {
    auto const all_codes{
        context.semantic.definitions()
                    .linear_quantized
                    .bit_width[context.semantic.definitions().linear_quantized.index(quantized)] ==
                64
            ? (std::numeric_limits<std::uint64_t>::max)()
            : (
                  std::uint64_t{1}
                  << context.semantic.definitions().linear_quantized.bit_width
                         [context.semantic.definitions().linear_quantized.index(quantized)]) -
                  1};
    return all_codes -
           context.semantic.definitions()
               .linear_quantized
               .reserved_codes[context.semantic.definitions().linear_quantized.index(quantized)];
}

auto packed_field_layouts(CppIR& ir,
                          schema::TypeDefinitionId packed,
                          schema::TypeGraph const& graph,
                          int storage_bits) -> std::vector<schema::PackedSegmentId> {
    auto const all_bits{storage_bits == 64 ? std::numeric_limits<std::uint64_t>::max()
                                           : (std::uint64_t{1} << storage_bits) - 1};
    auto const& definitions{graph.definitions()};
    auto const& fields{definitions.packed_fields};
    auto const& occurrences{graph.occurrences()};
    auto& computed{ir.lowering.packed_field_layouts};
    computed.prepare(fields.name.size());
    std::vector<schema::PackedSegmentId> layouts;
    auto const segments{definitions.packed.segments_of(packed)};
    layouts.reserve(segments.size());
    auto const most_significant_first{
        definitions.packed.bit_order[definitions.packed.index(packed)] ==
        PackedBitOrder::most_significant_first};
    auto offset{most_significant_first ? storage_bits : 0};
    for (auto const segment : segments) {
        auto const is_field{segment.kind() == schema::PackedSegmentKind::field};
        auto const bits{is_field ? fields.bit_width[fields.index(segment)]
                                 : definitions.packed_reserved_bits
                                       .bit_width[definitions.packed_reserved_bits.index(segment)]};
        if (most_significant_first) {
            offset -= static_cast<int>(bits);
        }
        if (is_field) {
            auto const source_row{fields.index(segment)};
            auto const occurrence{occurrences.index(fields.semantic_type[source_row])};
            auto const definition{graph.definition(occurrences.type[occurrence])};
            auto const kind{definition.kind()};
            auto const scalar{fields.bit_width_auto[source_row] ||
                                      kind == schema::TypeDefinitionKind::integer_scalar
                                  ? schema::integer_domain(graph, occurrences.type[occurrence])
                                  : std::nullopt};
            auto type{occurrences.cpp_type[occurrence]};
            if (scalar || kind == schema::TypeDefinitionKind::linear_quantized ||
                kind == schema::TypeDefinitionKind::fixed_point ||
                kind == schema::TypeDefinitionKind::mini_float) {
                auto const signedness{
                    scalar ? definitions.integer_scalars
                                 .signedness[definitions.integer_scalars.index(*scalar)]
                    : kind == schema::TypeDefinitionKind::fixed_point
                        ? definitions.fixed_points
                              .signedness[definitions.fixed_points.index(definition)]
                        : false};
                type = ir.types.intern(fixed_width_integer_spelling(signedness, bits));
            }
            auto const row{computed.index(segment)};
            computed.type[row] = type;
            computed.definition[row] = definition;
            computed.boolean[row] = ir.types.spelling(type) == "bool";
            computed.offset[row] = static_cast<std::uint32_t>(offset);
            computed.value_mask[row] = bits == 64 ? all_bits : (std::uint64_t{1} << bits) - 1;
            layouts.push_back(segment);
        }
        if (!most_significant_first) {
            offset += static_cast<int>(bits);
        }
    }
    return layouts;
}

auto enum_count_value(LoweringContext& context, schema::TypeDefinitionId const& type)
    -> std::optional<std::uint64_t> {
    auto const& definitions{context.semantic.definitions()};
    auto const row{definitions.enums.index(type)};
    auto const domain{definitions.enums.domain[row]};
    auto const& domains{context.semantic.enum_domains()};
    auto const values{domains.values(domain)};
    if (definitions.enums.enum_array[row]) {
        return values.size();
    }
    auto const count_value{domains.count_value[domains.index(domain)]};
    if (count_value.value == 0) {
        return std::nullopt;
    }
    auto const count{values.size()};
    for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
        if (values[ordinal] != count_value) {
            continue;
        }
        auto const code{domains.code(domain, ordinal)};
        return code && !code->negative ? std::optional{code->magnitude} : std::nullopt;
    }
    return std::nullopt;
}

auto enum_range_check(LoweringContext& context,
                      schema::TypeDefinitionId const& type,
                      std::string const& field_alias,
                      std::string const& value) -> std::optional<std::string> {
    if (context.semantic.definitions()
            .enums.enum_array[context.semantic.definitions().enums.index(type)]) {
        return "static_cast<std::size_t>(" + value + ") < ioj::enum_count<" + field_alias + ">()";
    }
    if (auto const count{enum_count_value(context, type)}; count.has_value()) {
        return "static_cast<std::uint64_t>(" + value + ") < " + std::to_string(*count);
    }
    return std::nullopt;
}

auto invalid_value_may_be_constructed(LoweringContext& context,
                                      std::optional<std::uint64_t> const invalid_value,
                                      std::span<schema::PackedSegmentId const> fields) -> bool {
    auto& packed_store{context.ir.lowering.packed_field_layouts};
    if (!invalid_value.has_value()) {
        return false;
    }

    for (auto const packed_field : fields) {
        auto const encoded_value{
            (*invalid_value >> packed_store.offset[packed_store.index(packed_field)]) &
            packed_store.value_mask[packed_store.index(packed_field)]};
        if (packed_store.definition[packed_store.index(packed_field)].kind() ==
            schema::TypeDefinitionKind::linear_quantized) {
            if (encoded_value >
                maximum_quantized_code(context,
                                       packed_store.definition[packed_store.index(packed_field)])) {
                return false;
            }
            continue;
        }
        if (packed_store.definition[packed_store.index(packed_field)].kind() !=
            schema::TypeDefinitionKind::enumeration) {
            continue;
        }
        auto const count{
            enum_count_value(context, packed_store.definition[packed_store.index(packed_field)])};
        if (!count.has_value()) {
            continue;
        }
        if (encoded_value >= *count) {
            return false;
        }
    }

    return true;
}

void append_make_parameters(LoweringContext& context,
                            std::string& output,
                            std::span<schema::PackedSegmentId const> fields) {
    for (std::size_t index{}; index < fields.size(); ++index) {
        if (index != 0) {
            output += ", ";
        }
        output += packed_field_type_alias(context, fields[index]) + " const " +
                  packed_field_value_name(context, fields[index]);
    }
}

void append_semantic_range_assertion(LoweringContext& context,
                                     std::string& output,
                                     schema::PackedSegmentId const& field,
                                     std::string const& value_name) {
    if (!context.semantic.definitions()
             .packed_fields.minimum_value[context.semantic.definitions().packed_fields.index(field)]
             .has_value()) {
        return;
    }

    auto const field_alias{packed_field_type_alias(context, field)};
    auto const minimum{
        context.semantic.definitions()
                    .packed_fields
                    .kind[context.semantic.definitions().packed_fields.index(field)] ==
                PackedFieldKind::unsigned_integer
            ? context.text(context.semantic.definitions()
                               .packed_fields
                               .name[context.semantic.definitions().packed_fields.index(field)]) +
                  "_minimum"
            : integer_cast_literal(
                  field_alias,
                  *context.semantic.definitions()
                       .packed_fields
                       .minimum_value[context.semantic.definitions().packed_fields.index(field)])};
    auto const maximum{
        context.semantic.definitions()
                    .packed_fields
                    .kind[context.semantic.definitions().packed_fields.index(field)] ==
                PackedFieldKind::unsigned_integer
            ? context.text(context.semantic.definitions()
                               .packed_fields
                               .name[context.semantic.definitions().packed_fields.index(field)]) +
                  "_maximum"
            : integer_cast_literal(
                  field_alias,
                  *context.semantic.definitions()
                       .packed_fields
                       .maximum_value[context.semantic.definitions().packed_fields.index(field)])};
    output += "        assert((" + value_name + " >= " + minimum + " && " + value_name +
              " <= " + maximum + ")";
    for (auto const code_id : context.declarations.items(
             context.semantic.definitions()
                 .packed_fields
                 .named_codes[context.semantic.definitions().packed_fields.index(field)])) {
        auto const code{code_id};
        if (context.declarations.packed_named_codes
                .sentinel[context.declarations.packed_named_codes.index(code)]) {
            output +=
                " || " + value_name + " == " +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_" +
                context.text(context.declarations.packed_named_codes
                                 .name[context.declarations.packed_named_codes.index(code)]);
        }
    }
    output += ");\n";
}

void append_field_validation(LoweringContext& context,
                             std::string& output,
                             schema::PackedSegmentId packed_field,
                             std::string const& value_name) {
    auto& packed_store{context.ir.lowering.packed_field_layouts};
    auto const packed_store_index{packed_store.index(packed_field)};
    auto const& field{packed_field};
    auto const field_alias{packed_field_type_alias(context, field)};
    if (packed_store.boolean[packed_store_index]) {
        return;
    }
    if (packed_store.definition[packed_store_index].kind() ==
        schema::TypeDefinitionKind::linear_quantized) {
        output +=
            "        assert(" + value_name + " <= " +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_maximum_encoded);\n";
        return;
    }
    if (packed_store.definition[packed_store_index].kind() ==
        schema::TypeDefinitionKind::fixed_point) {
        output +=
            "        assert(" + value_name + " >= " +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_minimum_allowed_raw && " + value_name + " <= " +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_maximum_allowed_raw);\n";
        return;
    }
    if (packed_store.definition[packed_store_index].kind() ==
        schema::TypeDefinitionKind::mini_float) {
        output +=
            "        assert(" + value_name + " <= " +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_maximum_encoded);\n";
        return;
    }
    if (context.semantic.definitions()
            .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] ==
        PackedFieldKind::enumeration) {
        if (packed_store.definition[packed_store_index].kind() ==
            schema::TypeDefinitionKind::enumeration) {
            if (auto const check{enum_range_check(
                    context, packed_store.definition[packed_store_index], field_alias, value_name)};
                check.has_value()) {
                output += "        assert(" + *check + ");\n";
                return;
            }
        }
        output +=
            "        assert(static_cast<" +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_underlying_type>(" + value_name + ") <= static_cast<" +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_underlying_type>(" +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_field::value_mask));\n";
        return;
    }
    if (context.semantic.definitions()
            .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] ==
        PackedFieldKind::signed_integer) {
        output +=
            "        assert(" + value_name + " >= " +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_minimum && " + value_name + " <= " +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_maximum);\n";
    } else if (!context.semantic.definitions()
                    .packed_fields
                    .minimum_value[context.semantic.definitions().packed_fields.index(field)]
                    .has_value()) {
        output +=
            "        assert(" + value_name + " <= " +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_maximum);\n";
    }
    append_semantic_range_assertion(context, output, field, value_name);
}

auto packed_field_expression(LoweringContext& context, schema::PackedSegmentId packed_field)
    -> std::string {
    auto const& field{packed_field};
    auto const value_name{packed_field_value_name(context, field)};
    return "ioj::packed_pack<" +
           context.text(
               context.semantic.definitions()
                   .packed_fields.name[context.semantic.definitions().packed_fields.index(field)]) +
           "_field>(" + value_name + ")";
}

auto packed_raw_value_expression(LoweringContext& context,
                                 std::span<schema::PackedSegmentId const> fields) -> std::string {
    std::string expression{"static_cast<storage_type>(\n"};
    for (std::size_t index{}; index < fields.size(); ++index) {
        expression += "            " + packed_field_expression(context, fields[index]);
        expression += index + 1 < fields.size() ? " |\n" : ")";
    }
    return expression;
}

void append_construction(LoweringContext& context,
                         std::string& output,
                         DeclarationHandle schema,
                         std::span<schema::PackedSegmentId const> fields) {
    output += "    explicit constexpr " +
              context.text(context.declarations.packed_values
                               .name[context.declarations.packed_values.index(schema)]) +
              "(";
    append_make_parameters(context, output, fields);
    output += ") noexcept {\n";
    for (auto const packed_field : fields) {
        append_field_validation(
            context, output, packed_field, packed_field_value_name(context, packed_field));
    }

    auto const raw_value{packed_raw_value_expression(context, fields)};
    if (invalid_value_may_be_constructed(
            context,
            context.declarations.packed_values
                .invalid_value[context.declarations.packed_values.index(schema)],
            fields)) {
        output += "        auto const raw{" + raw_value + "};\n";
        output += "        assert(raw != invalid_value);\n";
        output += "        value_ = raw;\n    }\n\n";
        return;
    }
    output += "        value_ = " + raw_value + ";\n    }\n\n";
}

auto packed_value_text(LoweringContext& context,
                       DeclarationHandle schema,
                       schema::TypeDefinitionId const& packed,
                       schema::TypeGraph const& type_graph) -> NodeId {
    auto& packed_store{context.ir.lowering.packed_field_layouts};
    auto& ir{context.ir};
    auto const storage{type_graph.occurrences().cpp_type[type_graph.occurrences().index(
        type_graph.definitions()
            .packed.storage_type[type_graph.definitions().packed.index(packed)])]};
    auto const storage_bits{*packed_unsigned_width(ir.types.spelling(storage))};
    auto const field_layouts{packed_field_layouts(ir, packed, type_graph, storage_bits)};

    std::vector<TypeDependencyId> dependencies{
        ir.types.dependency("ioj::PackedField", "ioj/packed_value.h"),
        ir.types.dependency("assert", "cassert"),
        ir.types.dependency("std::strong_ordering", "compare"),
        ir.types.dependency("std::numeric_limits", "limits"),
        ir.types.dependency("std::is_enum_v", "type_traits"),
    };
    dependencies.insert(dependencies.end(),
                        ir.types.dependencies(storage).begin(),
                        ir.types.dependencies(storage).end());
    if (auto dependency{dependency_for_integer(ir, storage)}) {
        dependencies.push_back(std::move(*dependency));
    }

    auto const export_specifier{
        context.declarations.packed_values
            .export_specifier[context.declarations.packed_values.index(schema)]};
    std::string output{"struct "};
    if (export_specifier.value != 0) {
        output += context.text(export_specifier);
        output += ' ';
    }
    output += context.text(context.declarations.packed_values
                               .name[context.declarations.packed_values.index(schema)]) +
              " {\n";
    output += "    using storage_type = " + std::string{ir.types.spelling(storage)} + ";\n";
    output += "    static_assert(ioj::valid_packed_storage<storage_type, " +
              std::to_string(storage_bits) + ">());\n";

    for (auto const layout : field_layouts) {
        auto const* field{&layout};
        auto const field_bits{
            context.semantic.definitions()
                .packed_fields
                .bit_width[context.semantic.definitions().packed_fields.index(layout)]};
        auto const offset{packed_store.offset[packed_store.index(layout)]};
        auto const field_type{packed_store.type[packed_store.index(layout)]};
        auto const field_alias{packed_field_type_alias(context, *field)};
        auto const quantized_type{packed_store.definition[packed_store.index(layout)]};
        auto const fixed_type{packed_store.definition[packed_store.index(layout)]};
        dependencies.insert(dependencies.end(),
                            ir.types.dependencies(field_type).begin(),
                            ir.types.dependencies(field_type).end());
        if (auto dependency{dependency_for_integer(ir, field_type)}) {
            dependencies.push_back(std::move(*dependency));
        }
        output +=
            "\n    // " +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(*field)]) +
            "\n";
        output +=
            "    using " + field_alias + " = " + std::string{ir.types.spelling(field_type)} + ";\n";
        if (context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(*field)] ==
            PackedFieldKind::enumeration) {
            output += "    static_assert(ioj::valid_packed_enum<" + field_alias + ", " +
                      std::to_string(field_bits) + ">());\n";
            output += "    using " +
                      context.text(
                          context.semantic.definitions()
                              .packed_fields
                              .name[context.semantic.definitions().packed_fields.index(*field)]) +
                      "_underlying_type = std::underlying_type_t<" + field_alias + ">;\n";
        } else if (context.semantic.definitions()
                       .packed_fields
                       .kind[context.semantic.definitions().packed_fields.index(*field)] ==
                   PackedFieldKind::signed_integer) {
            output += "    static_assert(std::is_signed_v<" + field_alias + ">);\n";
            output += "    static_assert(std::numeric_limits<" + field_alias +
                      ">::digits + 1 >= " + std::to_string(field_bits) + ");\n";
        } else if (context.semantic.definitions()
                           .packed_fields
                           .kind[context.semantic.definitions().packed_fields.index(*field)] ==
                       PackedFieldKind::linear_quantized ||
                   context.semantic.definitions()
                           .packed_fields
                           .kind[context.semantic.definitions().packed_fields.index(*field)] ==
                       PackedFieldKind::mini_float) {
            output += "    static_assert(std::is_unsigned_v<" + field_alias + ">);\n";
            output += "    static_assert(std::numeric_limits<" + field_alias +
                      ">::digits >= " + std::to_string(field_bits) + ");\n";
        } else if (context.semantic.definitions()
                       .packed_fields
                       .kind[context.semantic.definitions().packed_fields.index(*field)] ==
                   PackedFieldKind::fixed_point) {
            output +=
                "    static_assert(std::is_" +
                std::string{context.semantic.definitions().fixed_points.signedness
                                    [context.semantic.definitions().fixed_points.index(fixed_type)]
                                ? "signed"
                                : "unsigned"} +
                "_v<" + field_alias + ">);\n";
            output +=
                "    static_assert(std::numeric_limits<" + field_alias + ">::digits" +
                (context.semantic.definitions()
                         .fixed_points
                         .signedness[context.semantic.definitions().fixed_points.index(fixed_type)]
                     ? " + 1"
                     : "") +
                " >= " + std::to_string(field_bits) + ");\n";
        }

        auto const value_mask{packed_store.value_mask[packed_store.index(layout)]};
        output +=
            "    using " +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(*field)]) +
            "_field = ioj::PackedField<storage_type, " + packed_field_type_alias(context, *field) +
            ", " + std::to_string(offset) + ", " + std::to_string(field_bits) + ">;\n";
        if (quantized_type.kind() == schema::TypeDefinitionKind::linear_quantized) {
            output += "    inline static constexpr " + packed_field_type_alias(context, *field) +
                      " " +
                      context.text(
                          context.semantic.definitions()
                              .packed_fields
                              .name[context.semantic.definitions().packed_fields.index(*field)]) +
                      "_maximum_encoded{" + packed_field_type_alias(context, *field) + "{" +
                      hex_value(maximum_quantized_code(context, quantized_type)) + "}};\n";
        }
        if (context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(*field)] ==
            PackedFieldKind::mini_float) {
            output += "    inline static constexpr " + packed_field_type_alias(context, *field) +
                      " " +
                      context.text(
                          context.semantic.definitions()
                              .packed_fields
                              .name[context.semantic.definitions().packed_fields.index(*field)]) +
                      "_maximum_encoded{" + packed_field_type_alias(context, *field) + "{" +
                      hex_value(value_mask) + "}};\n";
        }
        if (fixed_type.kind() == schema::TypeDefinitionKind::fixed_point) {
            dependencies.push_back(ir.types.dependency("std::isfinite", "cmath"));
            auto const raw_type{packed_field_type_alias(context, *field)};
            if (context.semantic.definitions()
                    .fixed_points
                    .signedness[context.semantic.definitions().fixed_points.index(fixed_type)]) {
                auto const native_width{
                    *packed_signed_width(std::string{ir.types.spelling(field_type)})};
                if (field_bits == static_cast<std::uint32_t>(native_width)) {
                    output +=
                        "    inline static constexpr " + raw_type + " " +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(*field)]) +
                        "_minimum_raw{std::numeric_limits<" + raw_type + ">::min()};\n";
                    output +=
                        "    inline static constexpr " + raw_type + " " +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(*field)]) +
                        "_maximum_raw{std::numeric_limits<" + raw_type + ">::max()};\n";
                } else {
                    auto const sign_magnitude{std::uint64_t{1} << (field_bits - 1)};
                    output +=
                        "    inline static constexpr " + raw_type + " " +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(*field)]) +
                        "_minimum_raw{static_cast<" + raw_type + ">(-" +
                        std::to_string(sign_magnitude) + ")};\n";
                    output +=
                        "    inline static constexpr " + raw_type + " " +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(*field)]) +
                        "_maximum_raw{static_cast<" + raw_type + ">(" +
                        std::to_string(sign_magnitude - 1) + ")};\n";
                }
            } else {
                output +=
                    "    inline static constexpr " + raw_type + " " +
                    context.text(
                        context.semantic.definitions()
                            .packed_fields
                            .name[context.semantic.definitions().packed_fields.index(*field)]) +
                    "_minimum_raw{" + raw_type + "{0}};\n";
                output +=
                    "    inline static constexpr " + raw_type + " " +
                    context.text(
                        context.semantic.definitions()
                            .packed_fields
                            .name[context.semantic.definitions().packed_fields.index(*field)]) +
                    "_maximum_raw{" + raw_type + "{" + hex_value(value_mask) + "}};\n";
            }
            output +=
                "    inline static constexpr " + raw_type + " " +
                context.text(
                    context.semantic.definitions()
                        .packed_fields
                        .name[context.semantic.definitions().packed_fields.index(*field)]) +
                "_minimum_allowed_raw{" +
                (context.semantic.definitions()
                         .fixed_points
                         .minimum_raw_value[context.semantic.definitions().fixed_points.index(
                             fixed_type)]
                         .has_value()
                     ? integer_cast_literal(
                           raw_type,
                           *context.semantic.definitions().fixed_points.minimum_raw_value
                                [context.semantic.definitions().fixed_points.index(fixed_type)])
                     : context.text(
                           context.semantic.definitions()
                               .packed_fields
                               .name[context.semantic.definitions().packed_fields.index(*field)]) +
                           "_minimum_raw") +
                "};\n";
            output +=
                "    inline static constexpr " + raw_type + " " +
                context.text(
                    context.semantic.definitions()
                        .packed_fields
                        .name[context.semantic.definitions().packed_fields.index(*field)]) +
                "_maximum_allowed_raw{" +
                (context.semantic.definitions()
                         .fixed_points
                         .maximum_raw_value[context.semantic.definitions().fixed_points.index(
                             fixed_type)]
                         .has_value()
                     ? integer_cast_literal(
                           raw_type,
                           *context.semantic.definitions().fixed_points.maximum_raw_value
                                [context.semantic.definitions().fixed_points.index(fixed_type)])
                     : context.text(
                           context.semantic.definitions()
                               .packed_fields
                               .name[context.semantic.definitions().packed_fields.index(*field)]) +
                           "_maximum_raw") +
                "};\n";
        }
        if (context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(*field)] ==
            PackedFieldKind::signed_integer) {
            auto const signed_width{
                *packed_signed_width(std::string{ir.types.spelling(field_type)})};
            if (field_bits == static_cast<std::uint32_t>(signed_width)) {
                output +=
                    "    inline static constexpr " + field_alias + " " +
                    context.text(
                        context.semantic.definitions()
                            .packed_fields
                            .name[context.semantic.definitions().packed_fields.index(*field)]) +
                    "_minimum{std::numeric_limits<" + field_alias + ">::min()};\n";
                output +=
                    "    inline static constexpr " + field_alias + " " +
                    context.text(
                        context.semantic.definitions()
                            .packed_fields
                            .name[context.semantic.definitions().packed_fields.index(*field)]) +
                    "_maximum{std::numeric_limits<" + field_alias + ">::max()};\n";
            } else {
                auto const sign_magnitude{std::uint64_t{1} << (field_bits - 1)};
                output +=
                    "    inline static constexpr " + field_alias + " " +
                    context.text(
                        context.semantic.definitions()
                            .packed_fields
                            .name[context.semantic.definitions().packed_fields.index(*field)]) +
                    "_minimum{static_cast<" + field_alias + ">(-" + std::to_string(sign_magnitude) +
                    ")};\n";
                output +=
                    "    inline static constexpr " + field_alias + " " +
                    context.text(
                        context.semantic.definitions()
                            .packed_fields
                            .name[context.semantic.definitions().packed_fields.index(*field)]) +
                    "_maximum{static_cast<" + field_alias + ">(" +
                    std::to_string(sign_magnitude - 1) + ")};\n";
            }
        }
        if (context.semantic.definitions()
                    .packed_fields
                    .kind[context.semantic.definitions().packed_fields.index(*field)] ==
                PackedFieldKind::unsigned_integer &&
            std::string{ir.types.spelling(field_type)} != "bool") {
            output +=
                "    inline static constexpr " + field_alias + " " +
                context.text(
                    context.semantic.definitions()
                        .packed_fields
                        .name[context.semantic.definitions().packed_fields.index(*field)]) +
                "_minimum{" +
                (context.semantic.definitions()
                         .packed_fields
                         .minimum_value[context.semantic.definitions().packed_fields.index(*field)]
                         .has_value()
                     ? integer_cast_literal(
                           field_alias,
                           *context.semantic.definitions().packed_fields.minimum_value
                                [context.semantic.definitions().packed_fields.index(*field)])
                     : "0") +
                "};\n";
            output +=
                "    inline static constexpr " + field_alias + " " +
                context.text(
                    context.semantic.definitions()
                        .packed_fields
                        .name[context.semantic.definitions().packed_fields.index(*field)]) +
                "_maximum{" +
                (context.semantic.definitions()
                             .packed_fields
                             .maximum_value[context.semantic.definitions().packed_fields.index(
                                 *field)]
                             .has_value() &&
                         context.semantic.definitions()
                                 .packed_fields
                                 .maximum_value[context.semantic.definitions().packed_fields.index(
                                     *field)]
                                 ->magnitude != value_mask
                     ? integer_cast_literal(
                           field_alias,
                           *context.semantic.definitions().packed_fields.maximum_value
                                [context.semantic.definitions().packed_fields.index(*field)])
                     : "static_cast<" + field_alias + ">(" +
                           context.text(context.semantic.definitions()
                                            .packed_fields.name[context.semantic.definitions()
                                                                    .packed_fields.index(*field)]) +
                           "_field::value_mask)") +
                "};\n";
        }
        for (auto const code_id : context.declarations.items(
                 context.semantic.definitions()
                     .packed_fields
                     .named_codes[context.semantic.definitions().packed_fields.index(*field)])) {
            auto const code{code_id};
            output += "    inline static constexpr " + field_alias + " " +
                      context.text(
                          context.semantic.definitions()
                              .packed_fields
                              .name[context.semantic.definitions().packed_fields.index(*field)]) +
                      "_" +
                      context.text(context.declarations.packed_named_codes
                                       .name[context.declarations.packed_named_codes.index(code)]) +
                      "{" +
                      integer_cast_literal(
                          field_alias,
                          context.declarations.packed_named_codes
                              .value[context.declarations.packed_named_codes.index(code)]) +
                      "};\n";
        }

        if (context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(*field)] ==
            PackedFieldKind::enumeration) {
            if (auto const enum_type{packed_store.definition[packed_store.index(layout)]};
                enum_type.kind() == schema::TypeDefinitionKind::enumeration) {
                auto const& domains{context.semantic.enum_domains()};
                auto const& enums{context.semantic.definitions().enums};
                auto const domain{enums.domain[enums.index(enum_type)]};
                auto const values{domains.values(domain)};
                auto const& enumerators{context.declarations.enumerators};
                bool has_opaque_value{};
                auto const count{values.size()};
                for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
                    if (!enumerators.sentinel[enumerators.index(values[ordinal])] &&
                        values[ordinal] != domains.count_value[domains.index(domain)] &&
                        !domains.code(domain, ordinal)) {
                        has_opaque_value = true;
                        break;
                    }
                }
                if (has_opaque_value) {
                    output += "\n    static_assert([]<auto... values>() consteval -> bool {\n";
                    output +=
                        "        return ((static_cast<" +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(*field)]) +
                        "_underlying_type>(values) <=\n";
                    output +=
                        "                 static_cast<" +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(*field)]) +
                        "_underlying_type>(" +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(*field)]) +
                        "_field::value_mask)) && ...);\n";
                    output += "    }.template operator()<\n";
                    bool first_value{true};
                    for (auto const enumerator_id : domains.values(domain)) {
                        auto const enumerator{enumerator_id};
                        if (enumerator == domains.count_value[domains.index(domain)]) {
                            continue;
                        }
                        if (!first_value) {
                            output += ",\n";
                        }
                        output += "        " + field_alias + "::" +
                                  context.text(enumerators.name[enumerators.index(enumerator)]);
                        first_value = false;
                    }
                    output += "\n    >());\n";
                }
            }
        }
    }

    if (context.declarations.packed_values
            .invalid_value[context.declarations.packed_values.index(schema)]
            .has_value()) {
        output += "\n    inline static constexpr storage_type invalid_value{storage_type{" +
                  hex_value(*context.declarations.packed_values
                                 .invalid_value[context.declarations.packed_values.index(schema)]) +
                  "}};\n";
    }

    output += "\n    constexpr " +
              context.text(context.declarations.packed_values
                               .name[context.declarations.packed_values.index(schema)]) +
              "() noexcept = " +
              (type_graph.definitions()
                       .packed.default_raw_value[type_graph.definitions().packed.index(packed)]
                       .has_value()
                   ? "default"
                   : "delete") +
              ";\n";
    output +=
        "    [[nodiscard]] static constexpr auto from_raw(storage_type const raw) noexcept -> " +
        context.text(context.declarations.packed_values
                         .name[context.declarations.packed_values.index(schema)]) +
        " {\n";
    if (type_graph.definitions()
            .packed.default_raw_value[type_graph.definitions().packed.index(packed)]
            .has_value()) {
        output += "        " +
                  context.text(context.declarations.packed_values
                                   .name[context.declarations.packed_values.index(schema)]) +
                  " result;\n";
        output += "        result.value_ = raw;\n        return result;\n    }\n\n";
    } else {
        output += "        return " +
                  context.text(context.declarations.packed_values
                                   .name[context.declarations.packed_values.index(schema)]) +
                  "{RawTag{}, raw};\n    }\n\n";
    }
    output += "    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type {\n";
    output += "        return value_;\n    }\n\n";
    append_construction(context, output, schema, field_layouts);

    output += "    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {\n";
    std::vector<std::string> validity_checks;
    if (context.declarations.packed_values
            .invalid_value[context.declarations.packed_values.index(schema)]
            .has_value()) {
        validity_checks.push_back("value_ != invalid_value");
    }
    for (auto const layout : field_layouts) {
        auto const& field{layout};
        if (context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] ==
            PackedFieldKind::linear_quantized) {
            validity_checks.push_back(
                packed_field_accessor(context, field) + "() <= " +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_maximum_encoded");
            continue;
        }
        if ((context.semantic.definitions().packed_fields.kind[context.semantic.definitions()
                                                                   .packed_fields.index(field)] ==
                 PackedFieldKind::unsigned_integer ||
             context.semantic.definitions().packed_fields.kind[context.semantic.definitions()
                                                                   .packed_fields.index(field)] ==
                 PackedFieldKind::signed_integer) &&
            context.semantic.definitions()
                .packed_fields
                .minimum_value[context.semantic.definitions().packed_fields.index(field)]
                .has_value()) {
            auto const field_alias{packed_field_type_alias(context, field)};
            auto const minimum{
                context.semantic.definitions()
                            .packed_fields
                            .kind[context.semantic.definitions().packed_fields.index(field)] ==
                        PackedFieldKind::unsigned_integer
                    ? context.text(
                          context.semantic.definitions()
                              .packed_fields
                              .name[context.semantic.definitions().packed_fields.index(field)]) +
                          "_minimum"
                    : integer_cast_literal(
                          field_alias,
                          *context.semantic.definitions().packed_fields.minimum_value
                               [context.semantic.definitions().packed_fields.index(field)])};
            auto const maximum{
                context.semantic.definitions()
                            .packed_fields
                            .kind[context.semantic.definitions().packed_fields.index(field)] ==
                        PackedFieldKind::unsigned_integer
                    ? context.text(
                          context.semantic.definitions()
                              .packed_fields
                              .name[context.semantic.definitions().packed_fields.index(field)]) +
                          "_maximum"
                    : integer_cast_literal(
                          field_alias,
                          *context.semantic.definitions().packed_fields.maximum_value
                               [context.semantic.definitions().packed_fields.index(field)])};
            auto check{
                "(" +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "() >= " + minimum + " && " +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "() <= " + maximum + ")"};
            for (auto const code_id : context.declarations.items(
                     context.semantic.definitions()
                         .packed_fields
                         .named_codes[context.semantic.definitions().packed_fields.index(field)])) {
                auto const code{code_id};
                if (context.declarations.packed_named_codes
                        .sentinel[context.declarations.packed_named_codes.index(code)]) {
                    check +=
                        " || " +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
                        "() == " +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
                        "_" +
                        context.text(
                            context.declarations.packed_named_codes
                                .name[context.declarations.packed_named_codes.index(code)]);
                }
            }
            validity_checks.push_back("(" + std::move(check) + ")");
            continue;
        }
        if (context.semantic.definitions()
                .packed_fields.kind[context.semantic.definitions().packed_fields.index(field)] !=
            PackedFieldKind::enumeration) {
            if (context.semantic.definitions()
                    .packed_fields
                    .kind[context.semantic.definitions().packed_fields.index(field)] ==
                PackedFieldKind::fixed_point) {
                auto const fixed_type{packed_store.definition[packed_store.index(layout)]};
                if (context.semantic.definitions()
                        .fixed_points
                        .minimum_raw_value[context.semantic.definitions().fixed_points.index(
                            fixed_type)]
                        .has_value() ||
                    context.semantic.definitions()
                        .fixed_points
                        .maximum_raw_value[context.semantic.definitions().fixed_points.index(
                            fixed_type)]
                        .has_value()) {
                    validity_checks.push_back(
                        "(" +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
                        "_raw() >= " +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
                        "_minimum_allowed_raw && " +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
                        "_raw() <= " +
                        context.text(
                            context.semantic.definitions()
                                .packed_fields
                                .name[context.semantic.definitions().packed_fields.index(field)]) +
                        "_maximum_allowed_raw)");
                }
            }
            continue;
        }
        auto const enum_type{packed_store.definition[packed_store.index(layout)]};
        if (enum_type.kind() == schema::TypeDefinitionKind::enumeration) {
            if (auto const check{enum_range_check(
                    context,
                    enum_type,
                    packed_field_type_alias(context, field),
                    context.text(context.semantic.definitions().packed_fields.name
                                     [context.semantic.definitions().packed_fields.index(field)]) +
                        "()")};
                check.has_value()) {
                validity_checks.push_back(*check);
            }
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
    output += "    [[nodiscard]] constexpr auto operator<=>(" +
              context.text(context.declarations.packed_values
                               .name[context.declarations.packed_values.index(schema)]) +
              " const&) const noexcept = default;\n";

    for (auto const field_layout : field_layouts) {
        auto const& field{field_layout};
        auto const field_alias{packed_field_type_alias(context, field)};
        auto const field_bits{
            context.semantic.definitions()
                .packed_fields
                .bit_width[context.semantic.definitions().packed_fields.index(field_layout)]};
        auto const accessor{packed_field_accessor(context, field)};
        auto const fixed_type{packed_store.definition[packed_store.index(field_layout)]};
        output += "\n    [[nodiscard]] constexpr auto " + accessor + "() const noexcept -> " +
                  field_alias + " {\n";
        output +=
            "        return ioj::packed_extract<" +
            context.text(context.semantic.definitions()
                             .packed_fields
                             .name[context.semantic.definitions().packed_fields.index(field)]) +
            "_field>(value_);\n";
        output += "    }\n";

        if (fixed_type.kind() == schema::TypeDefinitionKind::fixed_point) {
            auto const raw_type{packed_field_type_alias(context, field)};
            auto const fractional_bits{std::to_string(
                context.semantic.definitions()
                    .fixed_points.fractional_bits[context.semantic.definitions().fixed_points.index(
                        fixed_type)])};
            auto const exclusive_limit{std::to_string(
                context.semantic.definitions()
                        .fixed_points
                        .signedness[context.semantic.definitions().fixed_points.index(fixed_type)]
                    ? field_bits - 1
                    : field_bits)};
            output +=
                "\n    [[nodiscard]] auto " +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_value() const noexcept -> double {\n";
            output +=
                "        return std::ldexp(static_cast<double>(" +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_raw()), -" + fractional_bits + ");\n    }\n";
            output +=
                "\n    [[nodiscard]] static auto try_encode_" +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_value(double const value, " + raw_type + "& out_raw) noexcept -> bool {\n";
            output +=
                "        if (!std::isfinite(value)) {\n            return false;\n        }\n";
            output +=
                "        if (value < std::ldexp(static_cast<double>(" +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_minimum_allowed_raw), -" + fractional_bits + ") ||\n";
            output +=
                "            value > std::ldexp(static_cast<double>(" +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_maximum_allowed_raw), -" + fractional_bits + ")) {\n";
            output += "            return false;\n        }\n";
            output += "        auto const scaled{std::ldexp(value, " + fractional_bits + ")};\n";
            output +=
                "        if (!std::isfinite(scaled)) {\n            return false;\n        }\n";
            if (context.semantic.definitions()
                    .fixed_points
                    .rounding[context.semantic.definitions().fixed_points.index(fixed_type)] ==
                FixedPointRounding::toward_zero) {
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
            if (context.semantic.definitions()
                    .fixed_points
                    .signedness[context.semantic.definitions().fixed_points.index(fixed_type)]) {
                output += " ||\n            rounded < -std::ldexp(1.0, " + exclusive_limit + ")";
            } else {
                output += " || rounded < 0.0";
            }
            output += ") {\n            return false;\n        }\n";
            output += "        auto const raw{static_cast<" + raw_type + ">(rounded)};\n";
            output +=
                "        if (raw < " +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_minimum_allowed_raw || raw > " +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_maximum_allowed_raw) {\n";
            output += "            return false;\n        }\n";
            output += "        out_raw = raw;\n        return true;\n    }\n";
        }

        if (context.declarations.packed_values
                .mutable_value[context.declarations.packed_values.index(schema)]) {
            output += "\n    constexpr void set_" + accessor + "(" + field_alias +
                      " const value) noexcept {\n";
            append_field_validation(context, output, field_layout, "value");
            output +=
                "        value_ = ioj::packed_insert<" +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_field>(value_, value);\n";
            output += "    }\n";

            if (fixed_type.kind() == schema::TypeDefinitionKind::fixed_point) {
                output +=
                    "\n    void set_" +
                    context.text(
                        context.semantic.definitions()
                            .packed_fields
                            .name[context.semantic.definitions().packed_fields.index(field)]) +
                    "_value(double const value) noexcept {\n";
                output += "        " + packed_field_type_alias(context, field) + " raw{};\n";
                output +=
                    "        [[maybe_unused]] auto const success{try_encode_" +
                    context.text(
                        context.semantic.definitions()
                            .packed_fields
                            .name[context.semantic.definitions().packed_fields.index(field)]) +
                    "_value(value, raw)};\n";
                output +=
                    "        assert(success && \"Packed fixed-point value does not fit.\");\n";
                output +=
                    "        set_" +
                    context.text(
                        context.semantic.definitions()
                            .packed_fields
                            .name[context.semantic.definitions().packed_fields.index(field)]) +
                    "_raw(raw);\n    }\n";
            }
        }

        if (context.semantic.definitions()
                .packed_fields
                .range_helper[context.semantic.definitions().packed_fields.index(field)]) {
            dependencies.push_back(ir.types.dependency("ioj::range_fits", "ioj/range_fits.h"));
            output +=
                "\n    [[nodiscard]] static constexpr auto " +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_range_fits(" + field_alias + " const first, " + field_alias +
                " const count) noexcept -> bool {\n";
            output +=
                "        return ioj::range_fits(first, count, " +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_minimum, " +
                context.text(context.semantic.definitions()
                                 .packed_fields
                                 .name[context.semantic.definitions().packed_fields.index(field)]) +
                "_maximum);\n";
            output += "    }\n";
        }
    }

    output += "  private:\n";
    if (!type_graph.definitions()
             .packed.default_raw_value[type_graph.definitions().packed.index(packed)]
             .has_value()) {
        output += "    struct RawTag {};\n    constexpr " +
                  context.text(context.declarations.packed_values
                                   .name[context.declarations.packed_values.index(schema)]) +
                  "(RawTag, storage_type const raw) noexcept : value_{raw} {}\n";
    }
    output += "    storage_type value_{";
    auto const explicit_defaults{std::ranges::any_of(field_layouts, [&](auto const layout) {
        return context.semantic.definitions()
            .packed_fields.default_value[context.semantic.definitions().packed_fields.index(layout)]
            .has_value();
    })};
    output +=
        explicit_defaults
            ? hex_value(type_graph.definitions()
                            .packed.default_raw_value[type_graph.definitions().packed.index(packed)]
                            .value_or(0))
        : context.declarations.packed_values
                .invalid_value[context.declarations.packed_values.index(schema)]
                .has_value()
            ? "invalid_value"
            : "";
    output += "};\n};\n";
    output += "static_assert(ioj::valid_packed_value_layout<" +
              context.text(context.declarations.packed_values
                               .name[context.declarations.packed_values.index(schema)]) +
              ">());";

    return ir.raw(output, std::move(dependencies));
}

} // namespace

auto lower_packed_value(LoweringContext& context,
                        DeclarationHandle schema,
                        schema::TypeId const type_id) -> DeclarationEmission {
    auto const packed{context.semantic.definition(type_id)};
    return {.header = {packed_value_text(context, schema, packed, context.semantic)}};
}

} // namespace lispb::codegen::detail
