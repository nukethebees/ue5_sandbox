#include "declaration_emission.h"
#include "lowered_soa.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_internal.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/soa_types.h>
#include <lispb/schema/type_graph.h>
#include <lispb/strings.h>

#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto mask_field_width(LoweringContext& context, SoaMemberId member) -> std::string {
    if (context.declarations.soa_members
            .mask_dimensions[context.declarations.soa_members.index(member)]
            .empty()) {
        return "1";
    }
    std::vector<std::string> extents;
    extents.reserve(context.declarations.soa_members
                        .mask_dimensions[context.declarations.soa_members.index(member)]
                        .size());
    for (auto const dimension_id : context.declarations.items(
             context.declarations.soa_members
                 .mask_dimensions[context.declarations.soa_members.index(member)])) {
        auto const dimension{dimension_id};
        extents.push_back(
            "(" +
            context.text(context.declarations.soa_mask_dimensions
                             .extent[context.declarations.soa_mask_dimensions.index(dimension)]) +
            ")");
    }
    return join(extents, " * ");
}

auto mask_index_expression(LoweringContext& context, SoaMemberId member) -> std::string {
    std::vector<std::string> terms;
    auto const dimension_count{context.declarations.soa_members
                                   .mask_dimensions[context.declarations.soa_members.index(member)]
                                   .size()};
    terms.reserve(dimension_count);
    for (std::size_t index{}; index < dimension_count; ++index) {
        auto term{context.text(
            context.declarations.soa_mask_dimensions
                .index_name[context.declarations.soa_mask_dimensions.index(
                    context.declarations.items(context.declarations.soa_members
                                                   .mask_dimensions[context.declarations.soa_members
                                                                        .index(member)])[index])])};
        for (auto extent_index{index + 1}; extent_index < dimension_count; ++extent_index) {
            term +=
                " * (" +
                context.text(
                    context.declarations.soa_mask_dimensions.extent
                        [context.declarations.soa_mask_dimensions.index(context.declarations.items(
                            context.declarations.soa_members.mask_dimensions
                                [context.declarations.soa_members.index(member)])[extent_index])]) +
                ")";
        }
        terms.push_back(std::move(term));
    }
    return join(terms, " + ");
}

auto field_mask_nodes(LoweringContext& context, DeclarationHandle schema) -> Nodes {
    auto& ir{context.ir};
    if (!context.declarations.soas.field_mask_name[context.declarations.soas.index(schema)]
             .has_value()) {
        return {};
    }

    std::string const uint8_type{"std::uint8_t"};
    std::string const uint16_type{"std::uint16_t"};
    std::string const uint32_type{"std::uint32_t"};
    std::string const uint64_type{"std::uint64_t"};
    std::string const int32_type{"std::int32_t"};

    auto const mask_name{context.text(
        *context.declarations.soas.field_mask_name[context.declarations.soas.index(schema)])};
    auto const enum_name{context.text(
        *context.declarations.soas.field_enum_name[context.declarations.soas.index(schema)])};
    std::string output{"enum class " + enum_name + " : " + uint8_type + " {\n"};
    std::string previous_name;
    std::string previous_width;
    for (auto const member_id : context.declarations.items(
             context.declarations.soas.members[context.declarations.soas.index(schema)])) {
        auto const member{member_id};
        if (!context.declarations.soa_members
                 .mask_field[context.declarations.soa_members.index(member)]) {
            continue;
        }
        auto const name{title_case_identifier(
            context.text(context.declarations.soa_members
                             .name[context.declarations.soa_members.index(member)]))};
        auto const initializer{previous_name.empty() ? "0"
                                                     : "static_cast<" + uint8_type + ">(" +
                                                           previous_name + ") + " + previous_width};
        output += "    " + name + " = " + initializer + ",\n";
        previous_name = name;
        previous_width = mask_field_width(context, member);
    }
    output += "    Count = static_cast<" + uint8_type + ">(" + previous_name + ") + " +
              previous_width + ",\n};\n\n";

    output +=
        "struct " + mask_name +
        " {\n"
        "    inline static constexpr " +
        int32_type + " field_count{static_cast<" + int32_type + ">(" + enum_name +
        "::Count)};\n"
        "    static_assert(field_count <= 64, \"Field mask exceeds 64 bits.\");\n"
        "    using storage_type = std::conditional_t<\n"
        "        field_count <= 8,\n"
        "        " +
        uint8_type +
        ",\n"
        "        std::conditional_t<field_count <= 16,\n"
        "                           " +
        uint16_type +
        ",\n"
        "                           std::conditional_t<field_count <= 32, " +
        uint32_type + ", " + uint64_type +
        ">>>;\n\n"
        "    constexpr " +
        mask_name +
        "() noexcept = default;\n"
        "    explicit constexpr " +
        mask_name +
        "(storage_type const value) noexcept : value_{value} {}\n\n"
        "    [[nodiscard]] constexpr auto value() const noexcept -> storage_type { return value_; "
        "}\n"
        "    [[nodiscard]] constexpr auto is_empty() const noexcept -> bool { return value_ == 0; "
        "}\n"
        "    [[nodiscard]] constexpr auto has(" +
        enum_name +
        " const field) const noexcept -> bool {\n"
        "        return (value_ & bit(field)) != 0;\n"
        "    }\n"
        "    [[nodiscard]] static constexpr auto index(" +
        enum_name + " const field) noexcept -> " + int32_type +
        " {\n"
        "        return static_cast<" +
        int32_type +
        ">(field);\n"
        "    }\n"
        "    constexpr void set(" +
        enum_name +
        " const field) noexcept { value_ |= bit(field); }\n"
        "    constexpr void set(" +
        mask_name +
        " const fields) noexcept { value_ |= fields.value_; }\n"
        "    constexpr void clear(" +
        enum_name +
        " const field) noexcept {\n"
        "        value_ = static_cast<storage_type>(value_ & ~bit(field));\n"
        "    }\n";

    for (auto const member_id : context.declarations.items(
             context.declarations.soas.members[context.declarations.soas.index(schema)])) {
        auto const member{member_id};
        if (!context.declarations.soa_members
                 .mask_field[context.declarations.soa_members.index(member)] ||
            context.declarations.soa_members
                .mask_dimensions[context.declarations.soa_members.index(member)]
                .empty()) {
            continue;
        }
        output += "\n    [[nodiscard]] static constexpr auto " +
                  context.text(context.declarations.soa_members
                                   .name[context.declarations.soa_members.index(member)]) +
                  "_field(";
        for (std::size_t index{};
             index < context.declarations.soa_members
                         .mask_dimensions[context.declarations.soa_members.index(member)]
                         .size();
             ++index) {
            if (index > 0) {
                output += ", ";
            }
            output +=
                "" + int32_type + " const " +
                context.text(
                    context.declarations.soa_mask_dimensions
                        .index_name[context.declarations.soa_mask_dimensions.index(
                            context.declarations.items(
                                context.declarations.soa_members.mask_dimensions
                                    [context.declarations.soa_members.index(member)])[index])]);
        }
        output += ") noexcept -> " + enum_name +
                  " {\n"
                  "        return static_cast<" +
                  enum_name + ">(static_cast<" + int32_type + ">(" + enum_name + "::" +
                  title_case_identifier(
                      context.text(context.declarations.soa_members
                                       .name[context.declarations.soa_members.index(member)])) +
                  ") + " + mask_index_expression(context, member) +
                  ");\n"
                  "    }\n";
    }

    output += "  private:\n"
              "    [[nodiscard]] static constexpr auto bit(" +
              enum_name +
              " const field) noexcept -> storage_type {\n"
              "        return static_cast<storage_type>(" +
              uint64_type + "{1} << static_cast<" + uint8_type +
              ">(field));\n"
              "    }\n\n"
              "    storage_type value_{};\n"
              "};\n"
              "static_assert(sizeof(" +
              mask_name + ") == sizeof(" + mask_name +
              "::storage_type));\n"
              "static_assert(std::is_trivially_copyable_v<" +
              mask_name + ">);";

    return {ir.raw(std::move(output),
                   {ir.types.dependency("std::conditional_t", "type_traits"),
                    ir.types.dependency("std::uint64_t", "cstdint")})};
}

} // namespace

auto lower_soa_declaration(LoweringContext& context, DeclarationHandle schema)
    -> DeclarationEmission {
    auto& ir{context.ir};
    NodeListBuilder header{ir};
    header
        .add(ir.forward_declaration(ir.strings.intern(
            context.text(context.declarations.soas.name[context.declarations.soas.index(schema)]) +
            "Schema")))
        .new_lines(2);
    header.append(field_mask_nodes(context, schema)).new_lines(1);
    if (context.declarations.soas.layout_only[context.declarations.soas.index(schema)]) {
        return {.header = header.build(), .format_generated = true};
    }
    auto lowered{lower_single_allocation(context, schema)};
    header.append(lowered.header);
    return {.header = header.build(), .source = lowered.source, .format_generated = true};
}

} // namespace lispb::codegen::detail
