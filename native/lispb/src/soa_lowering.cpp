#include "declaration_emission.h"
#include "lowered_soa.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_emission.h"
#include "soa_internal.h"
#include <ioj/format_writer.h>

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/soa_mask_dimension_schemas.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/strings.h>

#include <cstddef>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto mask_field_width(LoweringContext& context, SoaMemberId member) -> std::string {
    auto const& declarations{context.declarations};
    auto const& members{declarations.soa_members};
    auto const& dimensions{declarations.soa_mask_dimensions};
    auto const selected{declarations.items(members.mask_dimensions[members.index(member)])};
    if (selected.empty()) {
        return "1";
    }
    std::vector<std::string> extents;
    extents.reserve(selected.size());
    for (auto const dimension : selected) {
        extents.push_back("(" + context.text(dimensions.extent[dimensions.index(dimension)]) + ")");
    }
    return join(extents, " * ");
}

auto mask_index_expression(LoweringContext& context, SoaMemberId member) -> std::string {
    auto const& declarations{context.declarations};
    auto const& members{declarations.soa_members};
    auto const& dimensions{declarations.soa_mask_dimensions};
    auto const selected{declarations.items(members.mask_dimensions[members.index(member)])};
    auto const count{selected.size()};
    std::vector<std::string> terms;
    terms.reserve(count);
    for (std::size_t index{}; index < count; ++index) {
        auto term{context.text(dimensions.index_name[dimensions.index(selected[index])])};
        for (auto extent_index{index + 1}; extent_index < count; ++extent_index) {
            term += " * (" +
                    context.text(dimensions.extent[dimensions.index(selected[extent_index])]) + ")";
        }
        terms.push_back(std::move(term));
    }
    return join(terms, " + ");
}

auto field_mask_nodes(LoweringContext& context, DeclarationHandle schema) -> Nodes {
    auto& ir{context.ir};
    auto const& declarations{context.declarations};
    auto const& soas{declarations.soas};
    auto const& members{declarations.soa_members};
    auto const& dimensions{declarations.soa_mask_dimensions};
    auto const row{soas.index(schema)};
    if (!soas.field_mask_name[row].has_value()) {
        return {};
    }

    auto const mask_name{context.text(*soas.field_mask_name[row])};
    auto const enum_name{context.text(*soas.field_enum_name[row])};
    auto const selected{declarations.items(soas.members[row])};
    std::string output;
    auto write{ioj::FormatWriter{std::back_inserter(output)}};
    write("enum class {} : std::uint8_t {{\n", enum_name);
    std::string previous_name;
    std::string previous_width;
    for (auto const member : selected) {
        auto const member_row{members.index(member)};
        if (!members.mask_field[member_row]) {
            continue;
        }
        auto const name{title_case_identifier(context.text(members.name[member_row]))};
        if (previous_name.empty()) {
            write("    {} = 0,\n", name);
        } else {
            write("    {} = static_cast<std::uint8_t>({}) + {},\n",
                  name,
                  previous_name,
                  previous_width);
        }
        previous_name = name;
        previous_width = mask_field_width(context, member);
    }
    write(
        "    Count = static_cast<std::uint8_t>({}) + {},\n}};\n\n", previous_name, previous_width);
    write(R"(struct {0} {{
    inline static constexpr std::int32_t field_count{{static_cast<std::int32_t>({1}::Count)}};
    static_assert(field_count <= 64, "Field mask exceeds 64 bits.");
    using storage_type = std::conditional_t<
        field_count <= 8,
        std::uint8_t,
        std::conditional_t<field_count <= 16,
                           std::uint16_t,
                           std::conditional_t<field_count <= 32, std::uint32_t, std::uint64_t>>>;

    constexpr {0}() noexcept = default;
    explicit constexpr {0}(storage_type const value) noexcept : value_{{value}} {{}}

    [[nodiscard]] constexpr auto value() const noexcept -> storage_type {{ return value_; }}
    [[nodiscard]] constexpr auto is_empty() const noexcept -> bool {{ return value_ == 0; }}
    [[nodiscard]] constexpr auto has({1} const field) const noexcept -> bool {{
        return (value_ & bit(field)) != 0;
    }}
    [[nodiscard]] static constexpr auto index({1} const field) noexcept -> std::int32_t {{
        return static_cast<std::int32_t>(field);
    }}
    constexpr void set({1} const field) noexcept {{ value_ |= bit(field); }}
    constexpr void set({0} const fields) noexcept {{ value_ |= fields.value_; }}
    constexpr void clear({1} const field) noexcept {{
        value_ = static_cast<storage_type>(value_ & ~bit(field));
    }}
)",
          mask_name,
          enum_name);

    for (auto const member : selected) {
        auto const member_row{members.index(member)};
        auto const selected_dimensions{declarations.items(members.mask_dimensions[member_row])};
        if (!members.mask_field[member_row] || selected_dimensions.empty()) {
            continue;
        }
        auto const name{context.text(members.name[member_row])};
        write("\n    [[nodiscard]] static constexpr auto {}_field(", name);
        auto const count{selected_dimensions.size()};
        for (std::size_t index{}; index < count; ++index) {
            write(
                "{}std::int32_t const {}",
                index > 0 ? ", " : "",
                context.text(dimensions.index_name[dimensions.index(selected_dimensions[index])]));
        }
        write(R"() noexcept -> {0} {{
        return static_cast<{0}>(static_cast<std::int32_t>({0}::{1}) + {2});
    }}
)",
              enum_name,
              title_case_identifier(name),
              mask_index_expression(context, member));
    }

    write(R"(  private:
    [[nodiscard]] static constexpr auto bit({1} const field) noexcept -> storage_type {{
        return static_cast<storage_type>(std::uint64_t{{1}} << static_cast<std::uint8_t>(field));
    }}

    storage_type value_{{}};
}};
static_assert(sizeof({0}) == sizeof({0}::storage_type));
static_assert(std::is_trivially_copyable_v<{0}>);)",
          mask_name,
          enum_name);

    return {ir.raw(std::move(output),
                   {ir.types.dependency("std::conditional_t", "type_traits"),
                    ir.types.dependency("std::uint64_t", "cstdint")})};
}

} // namespace

auto lower_soa_declaration(LoweringContext& context, DeclarationHandle schema)
    -> DeclarationEmission {
    auto& ir{context.ir};
    auto const& soas{context.declarations.soas};
    auto const row{soas.index(schema)};
    NodeListBuilder header{ir};
    header.add(ir.forward_declaration(ir.strings.intern(context.text(soas.name[row]) + "Schema")))
        .new_lines(1);
    auto const masks{field_mask_nodes(context, schema)};
    if (!masks.empty()) {
        header.new_lines(2).append(masks).new_lines(2);
    }
    if (soas.layout_only[row]) {
        header.new_lines(2).add(emit_single_allocation_column_group(context, schema));
        return {.header = header.build(), .format_generated = true};
    }
    auto lowered{lower_single_allocation(context, schema)};
    header.append(lowered.header);
    return {.header = header.build(), .source = lowered.source, .format_generated = true};
}

} // namespace lispb::codegen::detail
