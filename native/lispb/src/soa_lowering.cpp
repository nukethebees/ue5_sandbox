#include "declaration_emission.h"
#include "lowered_soa.h"
#include "lowering.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_api.h"
#include "soa_functions.h"
#include "soa_internal.h"
#include "soa_receiver.h"
#include "soa_representation.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/normal_module_schemas.h>
#include <codegen/schema/soa_allocator_variants.h>
#include <codegen/schema/soa_allocators.h>
#include <codegen/schema/soa_backend.h>
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

auto field_mask_nodes(LoweringContext& context,
                      DeclarationHandle schema,
                      bool const standard_library = false) -> Nodes {
    auto& ir{context.ir};
    if (!context.declarations.soas.field_mask_name[context.declarations.soas.index(schema)]
             .has_value()) {
        return {};
    }

    std::string const uint8_type{standard_library ? "std::uint8_t" : "uint8"};
    std::string const uint16_type{standard_library ? "std::uint16_t" : "uint16"};
    std::string const uint32_type{standard_library ? "std::uint32_t" : "uint32"};
    std::string const uint64_type{standard_library ? "std::uint64_t" : "uint64"};
    std::string const int32_type{standard_library ? "std::int32_t" : "int32"};

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

auto lower_soa_impl(LoweringContext& context,
                    DeclarationHandle schema,
                    Nodes storage_prelude,
                    std::map<StringId, DeclarationHandle> const* schemas = nullptr,
                    Nodes mutable_view_prelude = {},
                    Nodes view_prelude = {}) -> LoweredSoa {
    auto& ir{context.ir};
    auto const members{resolve_members(context, schema)};
    auto const view_name{
        context
            .optional_text(
                context.declarations.soas.view_name[context.declarations.soas.index(schema)])
            .value_or(context.text(
                          context.declarations.soas.name[context.declarations.soas.index(schema)]) +
                      "View")};
    auto const const_view_name{
        context
            .optional_text(
                context.declarations.soas.const_view_name[context.declarations.soas.index(schema)])
            .value_or(context.text(
                          context.declarations.soas.name[context.declarations.soas.index(schema)]) +
                      "ConstView")};
    SoaFunctions const functions{
        .const_view = soa_view_functions(context, members, true),
        .mutable_view = soa_view_functions(context, members, false),
        .operations = soa_storage_functions(context, schema, members),
        .permutations = soa_permutation_functions(context, members),
    };
    auto const_api{lower_soa_api(context,
                                 schema,
                                 SoaRepresentation::vector,
                                 SoaReceiver::const_view,
                                 const_view_name,
                                 schemas)};
    auto mutable_api{lower_soa_api(
        context, schema, SoaRepresentation::vector, SoaReceiver::mutable_view, view_name, schemas)};
    const_api.header.insert(const_api.header.end(), view_prelude.begin(), view_prelude.end());
    mutable_api.header.insert(mutable_api.header.end(), view_prelude.begin(), view_prelude.end());
    mutable_api.header.insert(
        mutable_api.header.end(), mutable_view_prelude.begin(), mutable_view_prelude.end());
    Nodes custom_source;
    auto storage{soa_storage_node(context,
                                  schema,
                                  members,
                                  view_name,
                                  const_view_name,
                                  functions,
                                  custom_source,
                                  std::move(storage_prelude),
                                  schemas)};

    NodeListBuilder header{ir};
    header
        .append(soa_view_struct_nodes(context,
                                      schema,
                                      members,
                                      view_name,
                                      const_view_name,
                                      functions,
                                      const_api,
                                      mutable_api,
                                      schemas))
        .add(storage);

    NodeListBuilder source{ir};
    bool has_source_definition{};
    auto add_definition = [&](FunctionId const spec, std::string const& owner) {
        if (has_source_definition) {
            source.new_lines(2);
        }
        source.add(ir.definition(spec, ir.strings.intern(owner)));
        has_source_definition = true;
    };
    source.append(std::move(custom_source)).new_lines(2);
    source.append(const_api.source).new_lines(2);
    source.append(mutable_api.source).new_lines(2);
    auto append_definitions = [&](std::span<FunctionId const> specs, std::string const& owner) {
        for (auto const& spec : specs) {
            add_definition(spec, owner);
        }
    };
    append_definitions(functions.const_view, const_view_name);
    append_definitions(functions.mutable_view, view_name);
    for (auto const spec : functions.operations) {
        if (!ir.function_flags(spec).define_in_header()) {
            add_definition(
                spec,
                context.text(
                    context.declarations.soas.name[context.declarations.soas.index(schema)]));
        }
    }
    add_definition(
        functions.permutations.front(),
        context.text(context.declarations.soas.name[context.declarations.soas.index(schema)]));
    append_definitions(
        functions.mutable_view,
        context.text(context.declarations.soas.name[context.declarations.soas.index(schema)]));
    return LoweredSoa{header.build(), source.build()};
}

auto lower_one_soa(LoweringContext& context,
                   DeclarationHandle schema,
                   std::map<StringId, DeclarationHandle> const& schemas,
                   SoaBackend const backend) -> LoweredSoa {
    auto& ir{context.ir};
    auto const& type_graph{context.semantic};
    auto const module_name{context.module};
    auto const standard_library{backend == SoaBackend::standard_library};
    if (context.declarations.soas.layout_only[context.declarations.soas.index(schema)]) {
        return {.header = field_mask_nodes(context, schema, standard_library)};
    }
    LoweredSoa lowered;
    if (context.declarations.soas.emits_vector_storage(schema)) {
        auto const id{type_graph.find_declared(
            module_name, context.declarations.soas.name[context.declarations.soas.index(schema)])};
        auto const constructor{id ? type_graph.definitions()
                                        .soas
                                        .equivalent_constructor[type_graph.definitions().soas.index(
                                            type_graph.definition(*id))]
                                        .value_or(StringId{})
                                  : StringId{}};
        lowered = standard_library
                    ? lower_native_soa(context, schema, schemas, false, {}, constructor)
                    : lower_soa_impl(context, schema, {}, &schemas);
    }
    if (context.declarations.soas.field_mask_name[context.declarations.soas.index(schema)]
            .has_value()) {
        NodeListBuilder header{ir};
        header.append(field_mask_nodes(context, schema, standard_library))
            .new_lines(2)
            .append(std::move(lowered.header));
        lowered.header = header.build();
    }
    if (context.declarations.soas.fixed[context.declarations.soas.index(schema)].has_value()) {
        NodeListBuilder header{ir};
        header.append(std::move(lowered.header))
            .new_lines(2)
            .append(lower_fixed_nodes(context, schema, schemas));
        lowered.header = header.build();
    }
    if (context.declarations.soas.single_allocation[context.declarations.soas.index(schema)]
            .has_value()) {
        NodeListBuilder header{ir};
        NodeListBuilder source{ir};
        header.append(std::move(lowered.header));
        source.append(std::move(lowered.source));
        auto compact{lower_single_allocation(context, schema, schemas, backend)};
        header.new_lines(2).append(std::move(compact.header));
        source.new_lines(2).append(std::move(compact.source));
        lowered = {header.build(), source.build()};
    }
    return lowered;
}

} // namespace

auto lower_soa(LoweringContext& context,
               DeclarationHandle schema,
               Nodes storage_prelude,
               Nodes mutable_view_prelude,
               Nodes view_prelude) -> LoweredSoa {
    return lower_soa_impl(context,
                          schema,
                          std::move(storage_prelude),
                          nullptr,
                          std::move(mutable_view_prelude),
                          std::move(view_prelude));
}

auto lower_soa_declaration(LoweringContext& context,
                           DeclarationHandle schema,
                           ModuleHandle module,
                           std::map<StringId, DeclarationHandle> const& schemas,
                           std::optional<SoaAllocatorVariantId> const allocator_variant)
    -> DeclarationEmission {
    if (allocator_variant && !context.declarations.soas.emits_vector_storage(schema)) {
        return {};
    }
    auto wanted{context.declarations.soas.name[context.declarations.soas.index(schema)]};
    auto variant_context{context};
    if (allocator_variant) {
        auto const variant{*allocator_variant};
        variant_context.soa_variant_prefix =
            context.declarations.soa_allocator_variants
                .prefix[context.declarations.soa_allocator_variants.index(variant)];
        wanted = context.ir.strings.intern(
            context.text(context.declarations.soa_allocator_variants
                             .prefix[context.declarations.soa_allocator_variants.index(variant)]) +
            context.text(context.declarations.soas.name[context.declarations.soas.index(schema)]));
    }
    auto const& item{schemas.at(wanted)};
    auto lowered{
        lower_one_soa(variant_context,
                      item,
                      schemas,
                      context.declarations.normal_modules
                          .soa_backend[context.declarations.normal_modules.index(module)])};
    NodeListBuilder header{context.ir};
    header
        .add(context.ir.forward_declaration(context.ir.strings.intern(
            context.text(context.declarations.soas.name[context.declarations.soas.index(item)]) +
            "Schema")))
        .new_lines(2)
        .append(std::move(lowered.header));
    return {.header = header.build(),
            .source = std::move(lowered.source),
            .format_generated =
                context.declarations.normal_modules
                        .soa_backend[context.declarations.normal_modules.index(module)] ==
                    SoaBackend::standard_library ||
                context.declarations.soas.single_allocation[context.declarations.soas.index(item)]
                    .has_value()};
}

} // namespace lispb::codegen::detail
