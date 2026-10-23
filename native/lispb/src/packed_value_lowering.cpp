#include "declaration_emission.h"
#include "lowering.h"
#include "lowering_context.h"
#include "packed_value_internal.h"
#include <ioj/format_writer.h>

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
    return std::format("0x{:x}", value);
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
    auto const spelling{ir.types.spelling(type)};
    if (spelling.starts_with("std::uint") || spelling.starts_with("std::int")) {
        return ir.types.dependency(spelling, "cstdint");
    }
    if (spelling.starts_with("uint") || spelling.starts_with("int")) {
        return ir.types.dependency(spelling, "CoreTypes.h");
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

auto packed_field_accessor(LoweringContext& context, schema::PackedSegmentId const& field)
    -> std::string {
    auto const& field_definitions{context.semantic.definitions().packed_fields};
    auto const row{field_definitions.index(field)};
    auto name{context.text(field_definitions.name[row])};
    auto const kind{field_definitions.kind[row]};
    if (kind == PackedFieldKind::linear_quantized || kind == PackedFieldKind::mini_float) {
        return name + "_encoded";
    }
    if (kind == PackedFieldKind::fixed_point) {
        return name + "_raw";
    }
    return name;
}

auto packed_field_type_alias(LoweringContext& context, schema::PackedSegmentId const& field)
    -> std::string {
    return packed_field_accessor(context, field) + "_type";
}

auto packed_field_value_name(LoweringContext& context, schema::PackedSegmentId const& field)
    -> std::string {
    return packed_field_accessor(context, field) + "_value";
}

auto maximum_quantized_code(LoweringContext& context, schema::TypeDefinitionId const quantized)
    -> std::uint64_t {
    auto const& quantized_types{context.semantic.definitions().linear_quantized};
    auto const row{quantized_types.index(quantized)};
    auto const bits{quantized_types.bit_width[row]};
    auto const all_codes{bits == 64 ? (std::numeric_limits<std::uint64_t>::max)()
                                    : (std::uint64_t{1} << bits) - 1};
    return all_codes - quantized_types.reserved_codes[row];
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
    auto const& enums{context.semantic.definitions().enums};
    if (enums.enum_array[enums.index(type)]) {
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
    auto const& packed_store{context.ir.lowering.packed_field_layouts};
    if (!invalid_value.has_value()) {
        return false;
    }

    for (auto const packed_field : fields) {
        auto const row{packed_store.index(packed_field)};
        auto const definition{packed_store.definition[row]};
        auto const encoded_value{(*invalid_value >> packed_store.offset[row]) &
                                 packed_store.value_mask[row]};
        if (definition.kind() == schema::TypeDefinitionKind::linear_quantized) {
            if (encoded_value > maximum_quantized_code(context, definition)) {
                return false;
            }
            continue;
        }
        if (definition.kind() != schema::TypeDefinitionKind::enumeration) {
            continue;
        }
        auto const count{enum_count_value(context, definition)};
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
    auto const& field_definitions{context.semantic.definitions().packed_fields};
    auto const& named_codes{context.declarations.packed_named_codes};
    auto const field_row{field_definitions.index(field)};
    auto const field_name{context.text(field_definitions.name[field_row])};
    auto const minimum_value{field_definitions.minimum_value[field_row]};
    if (!minimum_value.has_value()) {
        return;
    }

    auto const field_alias{packed_field_type_alias(context, field)};
    auto const unsigned_integer{field_definitions.kind[field_row] ==
                                PackedFieldKind::unsigned_integer};
    auto const minimum{unsigned_integer ? field_name + "_minimum"
                                        : integer_cast_literal(field_alias, *minimum_value)};
    auto const maximum{
        unsigned_integer
            ? field_name + "_maximum"
            : integer_cast_literal(field_alias, *field_definitions.maximum_value[field_row])};
    ioj::FormatWriter write{std::back_inserter(output)};
    write("        assert(({0} >= {1} && {0} <= {2})", value_name, minimum, maximum);
    for (auto const code : context.declarations.items(field_definitions.named_codes[field_row])) {
        auto const code_row{named_codes.index(code)};
        if (named_codes.sentinel[code_row]) {
            write(" || {} == {}_{}",
                  value_name,
                  field_name,
                  context.text(named_codes.name[code_row]));
        }
    }
    output += ");\n";
}

void append_field_validation(LoweringContext& context,
                             std::string& output,
                             schema::PackedSegmentId packed_field,
                             std::string const& value_name) {
    auto const& field_definitions{context.semantic.definitions().packed_fields};
    auto const& packed_store{context.ir.lowering.packed_field_layouts};
    auto const layout_row{packed_store.index(packed_field)};
    auto const field_row{field_definitions.index(packed_field)};
    auto const field_name{context.text(field_definitions.name[field_row])};
    auto const field_alias{packed_field_type_alias(context, packed_field)};
    auto const field_kind{field_definitions.kind[field_row]};
    auto const definition{packed_store.definition[layout_row]};
    auto const kind{definition.kind()};
    if (packed_store.boolean[layout_row]) {
        return;
    }

    ioj::FormatWriter write{std::back_inserter(output)};
    if (kind == schema::TypeDefinitionKind::linear_quantized ||
        kind == schema::TypeDefinitionKind::mini_float) {
        write("        assert({} <= {}_maximum_encoded);\n", value_name, field_name);
        return;
    }
    if (kind == schema::TypeDefinitionKind::fixed_point) {
        write("        assert({0} >= {1}_minimum_allowed_raw && {0} <= {1}_maximum_allowed_raw);\n",
              value_name,
              field_name);
        return;
    }
    if (field_kind == PackedFieldKind::enumeration) {
        if (kind == schema::TypeDefinitionKind::enumeration) {
            if (auto const check{enum_range_check(context, definition, field_alias, value_name)};
                check.has_value()) {
                write("        assert({});\n", *check);
                return;
            }
        }
        write("        assert(static_cast<{0}_underlying_type>({1}) <= "
              "static_cast<{0}_underlying_type>({0}_field::value_mask));\n",
              field_name,
              value_name);
        return;
    }
    if (field_kind == PackedFieldKind::signed_integer) {
        write(
            "        assert({0} >= {1}_minimum && {0} <= {1}_maximum);\n", value_name, field_name);
    } else if (!field_definitions.minimum_value[field_row].has_value()) {
        write("        assert({} <= {}_maximum);\n", value_name, field_name);
    }
    append_semantic_range_assertion(context, output, packed_field, value_name);
}

auto packed_field_expression(LoweringContext& context, schema::PackedSegmentId packed_field)
    -> std::string {
    auto const& field_definitions{context.semantic.definitions().packed_fields};
    auto const field_row{field_definitions.index(packed_field)};
    auto const field_name{context.text(field_definitions.name[field_row])};
    return std::format("ioj::packed_pack<{}_field>({})",
                       field_name,
                       packed_field_value_name(context, packed_field));
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
    auto const& packed_values{context.declarations.packed_values};
    auto const schema_row{packed_values.index(schema)};
    auto const name{context.text(packed_values.name[schema_row])};
    ioj::FormatWriter write{std::back_inserter(output)};
    write("    explicit constexpr {}(", name);
    append_make_parameters(context, output, fields);
    output += ") noexcept {\n";
    for (auto const packed_field : fields) {
        append_field_validation(
            context, output, packed_field, packed_field_value_name(context, packed_field));
    }

    auto const raw_value{packed_raw_value_expression(context, fields)};
    if (invalid_value_may_be_constructed(
            context, packed_values.invalid_value[schema_row], fields)) {
        write("        auto const raw{{{}}};\n", raw_value);
        output += "        assert(raw != invalid_value);\n";
        output += "        value_ = raw;\n    }\n\n";
        return;
    }
    write("        value_ = {};\n    }}\n\n", raw_value);
}

auto packed_value_text(LoweringContext& context,
                       DeclarationHandle schema,
                       schema::TypeDefinitionId const& packed,
                       schema::TypeGraph const& type_graph) -> NodeId {
    auto& ir{context.ir};
    auto const& definitions{type_graph.definitions()};
    auto const& field_definitions{definitions.packed_fields};
    auto const& fixed_points{definitions.fixed_points};
    auto const& packed_types{definitions.packed};
    auto const& occurrences{type_graph.occurrences()};
    auto const& packed_values{context.declarations.packed_values};
    auto const& named_codes{context.declarations.packed_named_codes};
    auto const schema_row{packed_values.index(schema)};
    auto const packed_row{packed_types.index(packed)};
    auto const name{context.text(packed_values.name[schema_row])};
    auto const invalid_value{packed_values.invalid_value[schema_row]};
    auto const default_raw{packed_types.default_raw_value[packed_row]};
    auto const storage{
        occurrences.cpp_type[occurrences.index(packed_types.storage_type[packed_row])]};
    auto const storage_bits{*packed_unsigned_width(ir.types.spelling(storage))};
    auto const field_layouts{packed_field_layouts(ir, packed, type_graph, storage_bits)};
    auto const& packed_store{ir.lowering.packed_field_layouts};

    std::vector<TypeDependencyId> dependencies{
        ir.types.dependency("ioj::PackedField", "ioj/packed_value.h"),
        ir.types.dependency("assert", "cassert"),
        ir.types.dependency("std::strong_ordering", "compare"),
        ir.types.dependency("std::numeric_limits", "limits"),
        ir.types.dependency("std::is_enum_v", "type_traits"),
    };
    auto const storage_dependencies{ir.types.dependencies(storage)};
    dependencies.insert(
        dependencies.end(), storage_dependencies.begin(), storage_dependencies.end());
    if (auto dependency{dependency_for_integer(ir, storage)}) {
        dependencies.push_back(*dependency);
    }

    auto const export_specifier{packed_values.export_specifier[schema_row]};
    std::string output{"struct "};
    ioj::FormatWriter write{std::back_inserter(output)};
    if (export_specifier.value != 0) {
        write("{} ", context.text(export_specifier));
    }
    write("{} {{\n"
          "    using storage_type = {};\n"
          "    static_assert(ioj::valid_packed_storage<storage_type, {}>());\n",
          name,
          ir.types.spelling(storage),
          storage_bits);

    for (auto const field : field_layouts) {
        auto const field_row{field_definitions.index(field)};
        auto const layout_row{packed_store.index(field)};
        auto const field_name{context.text(field_definitions.name[field_row])};
        auto const field_kind{field_definitions.kind[field_row]};
        auto const field_bits{field_definitions.bit_width[field_row]};
        auto const field_type{packed_store.type[layout_row]};
        auto const field_alias{packed_field_type_alias(context, field)};
        auto const definition{packed_store.definition[layout_row]};
        auto const kind{definition.kind()};
        auto const field_dependencies{ir.types.dependencies(field_type)};
        dependencies.insert(
            dependencies.end(), field_dependencies.begin(), field_dependencies.end());
        if (auto dependency{dependency_for_integer(ir, field_type)}) {
            dependencies.push_back(*dependency);
        }

        write("\n    // {}\n    using {} = {};\n",
              field_name,
              field_alias,
              ir.types.spelling(field_type));
        if (field_kind == PackedFieldKind::enumeration) {
            write("    static_assert(ioj::valid_packed_enum<{0}, {1}>());\n"
                  "    using {2}_underlying_type = std::underlying_type_t<{0}>;\n",
                  field_alias,
                  field_bits,
                  field_name);
        } else if (field_kind == PackedFieldKind::signed_integer) {
            write("    static_assert(std::is_signed_v<{0}>);\n"
                  "    static_assert(std::numeric_limits<{0}>::digits + 1 >= {1});\n",
                  field_alias,
                  field_bits);
        } else if (field_kind == PackedFieldKind::linear_quantized ||
                   field_kind == PackedFieldKind::mini_float) {
            write("    static_assert(std::is_unsigned_v<{0}>);\n"
                  "    static_assert(std::numeric_limits<{0}>::digits >= {1});\n",
                  field_alias,
                  field_bits);
        } else if (field_kind == PackedFieldKind::fixed_point) {
            auto const signedness{fixed_points.signedness[fixed_points.index(definition)]};
            write("    static_assert(std::is_{0}_v<{1}>);\n"
                  "    static_assert(std::numeric_limits<{1}>::digits{2} >= {3});\n",
                  signedness ? "signed" : "unsigned",
                  field_alias,
                  signedness ? " + 1" : "",
                  field_bits);
        }

        auto const value_mask{packed_store.value_mask[layout_row]};
        write("    using {}_field = ioj::PackedField<storage_type, {}, {}, {}>;\n",
              field_name,
              field_alias,
              packed_store.offset[layout_row],
              field_bits);
        if (kind == schema::TypeDefinitionKind::linear_quantized) {
            write("    inline static constexpr {0} {1}_maximum_encoded{{{0}{{{2}}}}};\n",
                  field_alias,
                  field_name,
                  hex_value(maximum_quantized_code(context, definition)));
        }
        if (field_kind == PackedFieldKind::mini_float) {
            write("    inline static constexpr {0} {1}_maximum_encoded{{{0}{{{2}}}}};\n",
                  field_alias,
                  field_name,
                  hex_value(value_mask));
        }
        if (kind == schema::TypeDefinitionKind::fixed_point) {
            dependencies.push_back(ir.types.dependency("std::isfinite", "cmath"));
            auto const fixed_row{fixed_points.index(definition)};
            if (fixed_points.signedness[fixed_row]) {
                auto const native_width{
                    *packed_signed_width(std::string{ir.types.spelling(field_type)})};
                if (field_bits == static_cast<std::uint32_t>(native_width)) {
                    write("    inline static constexpr {0} "
                          "{1}_minimum_raw{{std::numeric_limits<{0}>::min()}};\n"
                          "    inline static constexpr {0} "
                          "{1}_maximum_raw{{std::numeric_limits<{0}>::max()}};\n",
                          field_alias,
                          field_name);
                } else {
                    auto const sign_magnitude{std::uint64_t{1} << (field_bits - 1)};
                    write("    inline static constexpr {0} "
                          "{1}_minimum_raw{{static_cast<{0}>(-{2})}};\n"
                          "    inline static constexpr {0} "
                          "{1}_maximum_raw{{static_cast<{0}>({3})}};\n",
                          field_alias,
                          field_name,
                          sign_magnitude,
                          sign_magnitude - 1);
                }
            } else {
                write("    inline static constexpr {0} {1}_minimum_raw{{{0}{{0}}}};\n"
                      "    inline static constexpr {0} {1}_maximum_raw{{{0}{{{2}}}}};\n",
                      field_alias,
                      field_name,
                      hex_value(value_mask));
            }
            auto const minimum{fixed_points.minimum_raw_value[fixed_row]};
            auto const maximum{fixed_points.maximum_raw_value[fixed_row]};
            write("    inline static constexpr {0} {1}_minimum_allowed_raw{{{2}}};\n"
                  "    inline static constexpr {0} {1}_maximum_allowed_raw{{{3}}};\n",
                  field_alias,
                  field_name,
                  minimum ? integer_cast_literal(field_alias, *minimum)
                          : field_name + "_minimum_raw",
                  maximum ? integer_cast_literal(field_alias, *maximum)
                          : field_name + "_maximum_raw");
        }
        if (field_kind == PackedFieldKind::signed_integer) {
            auto const signed_width{
                *packed_signed_width(std::string{ir.types.spelling(field_type)})};
            if (field_bits == static_cast<std::uint32_t>(signed_width)) {
                write("    inline static constexpr {0} "
                      "{1}_minimum{{std::numeric_limits<{0}>::min()}};\n"
                      "    inline static constexpr {0} "
                      "{1}_maximum{{std::numeric_limits<{0}>::max()}};\n",
                      field_alias,
                      field_name);
            } else {
                auto const sign_magnitude{std::uint64_t{1} << (field_bits - 1)};
                write("    inline static constexpr {0} {1}_minimum{{static_cast<{0}>(-{2})}};\n"
                      "    inline static constexpr {0} {1}_maximum{{static_cast<{0}>({3})}};\n",
                      field_alias,
                      field_name,
                      sign_magnitude,
                      sign_magnitude - 1);
            }
        }
        if (field_kind == PackedFieldKind::unsigned_integer &&
            ir.types.spelling(field_type) != "bool") {
            auto const minimum{field_definitions.minimum_value[field_row]};
            auto const maximum{field_definitions.maximum_value[field_row]};
            write("    inline static constexpr {0} {1}_minimum{{{2}}};\n"
                  "    inline static constexpr {0} {1}_maximum{{{3}}};\n",
                  field_alias,
                  field_name,
                  minimum ? integer_cast_literal(field_alias, *minimum) : "0",
                  maximum && maximum->magnitude != value_mask
                      ? integer_cast_literal(field_alias, *maximum)
                      : std::format(
                            "static_cast<{}>({}_field::value_mask)", field_alias, field_name));
        }
        for (auto const code :
             context.declarations.items(field_definitions.named_codes[field_row])) {
            auto const code_row{named_codes.index(code)};
            write("    inline static constexpr {} {}_{}{{{}}};\n",
                  field_alias,
                  field_name,
                  context.text(named_codes.name[code_row]),
                  integer_cast_literal(field_alias, named_codes.value[code_row]));
        }

        if (field_kind == PackedFieldKind::enumeration &&
            kind == schema::TypeDefinitionKind::enumeration) {
            auto const& domains{context.semantic.enum_domains()};
            auto const& enums{definitions.enums};
            auto const& enumerators{context.declarations.enumerators};
            auto const domain{enums.domain[enums.index(definition)]};
            auto const count_value{domains.count_value[domains.index(domain)]};
            auto const values{domains.values(domain)};
            auto const count{values.size()};
            bool has_opaque_value{};
            for (std::size_t ordinal{}; ordinal < count; ++ordinal) {
                if (!enumerators.sentinel[enumerators.index(values[ordinal])] &&
                    values[ordinal] != count_value && !domains.code(domain, ordinal)) {
                    has_opaque_value = true;
                    break;
                }
            }
            if (has_opaque_value) {
                write("\n    static_assert([]<auto... values>() consteval -> bool {{\n"
                      "        return ((static_cast<{0}_underlying_type>(values) <=\n"
                      "                 static_cast<{0}_underlying_type>({0}_field::value_mask)) "
                      "&& ...);\n"
                      "    }}.template operator()<\n",
                      field_name);
                bool first_value{true};
                for (auto const enumerator : values) {
                    if (enumerator == count_value) {
                        continue;
                    }
                    if (!first_value) {
                        output += ",\n";
                    }
                    write("        {}::{}",
                          field_alias,
                          context.text(enumerators.name[enumerators.index(enumerator)]));
                    first_value = false;
                }
                output += "\n    >());\n";
            }
        }
    }

    if (invalid_value.has_value()) {
        write("\n    inline static constexpr storage_type invalid_value{{storage_type{{{}}}}};\n",
              hex_value(*invalid_value));
    }
    write("\n    constexpr {0}() noexcept = {1};\n"
          "    [[nodiscard]] static constexpr auto from_raw(storage_type const raw) noexcept -> "
          "{0} {{\n",
          name,
          default_raw.has_value() ? "default" : "delete");
    if (default_raw.has_value()) {
        write("        {} result;\n", name);
        output += "        result.value_ = raw;\n        return result;\n    }\n\n";
    } else {
        write("        return {}{{RawTag{{}}, raw}};\n    }}\n\n", name);
    }
    output += "    [[nodiscard]] constexpr auto raw_value() const noexcept -> storage_type {\n"
              "        return value_;\n    }\n\n";
    append_construction(context, output, schema, field_layouts);

    output += "    [[nodiscard]] constexpr auto is_valid() const noexcept -> bool {\n";
    std::vector<std::string> validity_checks;
    if (invalid_value.has_value()) {
        validity_checks.push_back("value_ != invalid_value");
    }
    for (auto const field : field_layouts) {
        auto const field_row{field_definitions.index(field)};
        auto const field_name{context.text(field_definitions.name[field_row])};
        auto const field_kind{field_definitions.kind[field_row]};
        if (field_kind == PackedFieldKind::linear_quantized) {
            validity_checks.push_back(std::format(
                "{}() <= {}_maximum_encoded", packed_field_accessor(context, field), field_name));
            continue;
        }
        if ((field_kind == PackedFieldKind::unsigned_integer ||
             field_kind == PackedFieldKind::signed_integer) &&
            field_definitions.minimum_value[field_row].has_value()) {
            auto const field_alias{packed_field_type_alias(context, field)};
            auto const unsigned_integer{field_kind == PackedFieldKind::unsigned_integer};
            auto const minimum{unsigned_integer
                                   ? field_name + "_minimum"
                                   : integer_cast_literal(
                                         field_alias, *field_definitions.minimum_value[field_row])};
            auto const maximum{unsigned_integer
                                   ? field_name + "_maximum"
                                   : integer_cast_literal(
                                         field_alias, *field_definitions.maximum_value[field_row])};
            auto check{std::format("({0}() >= {1} && {0}() <= {2})", field_name, minimum, maximum)};
            for (auto const code :
                 context.declarations.items(field_definitions.named_codes[field_row])) {
                auto const code_row{named_codes.index(code)};
                if (named_codes.sentinel[code_row]) {
                    std::format_to(std::back_inserter(check),
                                   " || {0}() == {0}_{1}",
                                   field_name,
                                   context.text(named_codes.name[code_row]));
                }
            }
            validity_checks.push_back("(" + std::move(check) + ")");
            continue;
        }
        auto const definition{packed_store.definition[packed_store.index(field)]};
        if (field_kind != PackedFieldKind::enumeration) {
            if (field_kind == PackedFieldKind::fixed_point) {
                auto const fixed_row{fixed_points.index(definition)};
                if (fixed_points.minimum_raw_value[fixed_row].has_value() ||
                    fixed_points.maximum_raw_value[fixed_row].has_value()) {
                    validity_checks.push_back(
                        std::format("({0}_raw() >= {0}_minimum_allowed_raw && {0}_raw() <= "
                                    "{0}_maximum_allowed_raw)",
                                    field_name));
                }
            }
            continue;
        }
        if (definition.kind() == schema::TypeDefinitionKind::enumeration) {
            if (auto const check{enum_range_check(context,
                                                  definition,
                                                  packed_field_type_alias(context, field),
                                                  field_name + "()")};
                check.has_value()) {
                validity_checks.push_back(*check);
            }
        }
    }
    output += "        return ";
    auto const check_count{validity_checks.size()};
    for (std::size_t index{}; index < check_count; ++index) {
        if (index != 0) {
            output += " && ";
        }
        output += validity_checks[index];
    }
    output += validity_checks.empty() ? "true;\n    }\n\n" : ";\n    }\n\n";
    write("    [[nodiscard]] constexpr auto operator<=>({} const&) const noexcept = default;\n",
          name);

    for (auto const field : field_layouts) {
        auto const field_row{field_definitions.index(field)};
        auto const field_name{context.text(field_definitions.name[field_row])};
        auto const field_alias{packed_field_type_alias(context, field)};
        auto const accessor{packed_field_accessor(context, field)};
        auto const definition{packed_store.definition[packed_store.index(field)]};
        auto const fixed_point{definition.kind() == schema::TypeDefinitionKind::fixed_point};
        write("\n    [[nodiscard]] constexpr auto {}() const noexcept -> {} {{\n"
              "        return ioj::packed_extract<{}_field>(value_);\n    }}\n",
              accessor,
              field_alias,
              field_name);

        if (fixed_point) {
            auto const fixed_row{fixed_points.index(definition)};
            auto const field_bits{field_definitions.bit_width[field_row]};
            auto const fractional_bits{fixed_points.fractional_bits[fixed_row]};
            auto const signedness{fixed_points.signedness[fixed_row]};
            auto const exclusive_limit{signedness ? field_bits - 1 : field_bits};
            write("\n    [[nodiscard]] auto {0}_value() const noexcept -> double {{\n"
                  "        return std::ldexp(static_cast<double>({0}_raw()), -{1});\n    }}\n"
                  "\n    [[nodiscard]] static auto try_encode_{0}_value(double const value, {2}& "
                  "out_raw) noexcept -> bool {{\n"
                  "        if (!std::isfinite(value)) {{\n            return false;\n        }}\n"
                  "        if (value < std::ldexp(static_cast<double>({0}_minimum_allowed_raw), "
                  "-{1}) ||\n"
                  "            value > std::ldexp(static_cast<double>({0}_maximum_allowed_raw), "
                  "-{1})) {{\n"
                  "            return false;\n        }}\n"
                  "        auto const scaled{{std::ldexp(value, {1})}};\n"
                  "        if (!std::isfinite(scaled)) {{\n            return false;\n        }}\n",
                  field_name,
                  fractional_bits,
                  field_alias);
            if (fixed_points.rounding[fixed_row] == FixedPointRounding::toward_zero) {
                output += "        auto const rounded{std::trunc(scaled)};\n";
            } else {
                output +=
                    "        auto const lower{std::floor(scaled)};\n"
                    "        auto const fraction{scaled - lower};\n"
                    "        auto const rounded{fraction > 0.5 ||\n"
                    "                           (fraction == 0.5 && std::fmod(lower, 2.0) != 0.0)\n"
                    "                               ? lower + 1.0\n"
                    "                               : lower};\n";
            }
            write("        if (rounded >= std::ldexp(1.0, {})", exclusive_limit);
            if (signedness) {
                write(" ||\n            rounded < -std::ldexp(1.0, {})", exclusive_limit);
            } else {
                output += " || rounded < 0.0";
            }
            write(") {{\n            return false;\n        }}\n"
                  "        auto const raw{{static_cast<{0}>(rounded)}};\n"
                  "        if (raw < {1}_minimum_allowed_raw || raw > {1}_maximum_allowed_raw) {{\n"
                  "            return false;\n        }}\n"
                  "        out_raw = raw;\n        return true;\n    }}\n",
                  field_alias,
                  field_name);
        }

        if (packed_values.mutable_value[schema_row]) {
            write(
                "\n    constexpr void set_{}({} const value) noexcept {{\n", accessor, field_alias);
            append_field_validation(context, output, field, "value");
            write("        value_ = ioj::packed_insert<{}_field>(value_, value);\n    }}\n",
                  field_name);
            if (fixed_point) {
                write("\n    void set_{0}_value(double const value) noexcept {{\n"
                      "        {1} raw{{}};\n"
                      "        [[maybe_unused]] auto const success{{try_encode_{0}_value(value, "
                      "raw)}};\n"
                      "        assert(success && \"Packed fixed-point value does not fit.\");\n"
                      "        set_{0}_raw(raw);\n    }}\n",
                      field_name,
                      field_alias);
            }
        }
        if (field_definitions.range_helper[field_row]) {
            dependencies.push_back(ir.types.dependency("ioj::range_fits", "ioj/range_fits.h"));
            write(
                "\n    [[nodiscard]] static constexpr auto {0}_range_fits({1} const first, {1} "
                "const count) noexcept -> bool {{\n"
                "        return ioj::range_fits(first, count, {0}_minimum, {0}_maximum);\n    }}\n",
                field_name,
                field_alias);
        }
    }

    output += "  private:\n";
    if (!default_raw.has_value()) {
        write("    struct RawTag {{}};\n    constexpr {}(RawTag, storage_type const raw) noexcept "
              ": value_{{raw}} {{}}\n",
              name);
    }
    auto const explicit_defaults{std::ranges::any_of(field_layouts, [&](auto const field) {
        return field_definitions.default_value[field_definitions.index(field)].has_value();
    })};
    write(
        "    storage_type value_{{{}}};\n}};\nstatic_assert(ioj::valid_packed_value_layout<{}>());",
        explicit_defaults           ? hex_value(default_raw.value_or(0))
        : invalid_value.has_value() ? "invalid_value"
                                    : "",
        name);
    return ir.raw(output, std::move(dependencies));
}

} // namespace

auto lower_packed_value(LoweringContext& context,
                        DeclarationHandle schema,
                        schema::TypeId const semantic_type) -> DeclarationEmission {
    auto const packed{context.semantic.definition(semantic_type)};
    return {.header = {packed_value_text(context, schema, packed, context.semantic)}};
}

} // namespace lispb::codegen::detail
