#include "lowering.h"
#include "lowering_utils.h"
#include "soa_internal.h"

#include <codegen/schema/soa_allocator_variants.h>

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <utility>

namespace codegen::detail {
namespace {

auto mask_field_width(SoaMemberSchema const& member) -> std::string {
    if (member.mask_dimensions.empty()) {
        return "1";
    }
    std::vector<std::string> extents;
    extents.reserve(member.mask_dimensions.size());
    for (auto const& dimension : member.mask_dimensions) {
        extents.push_back("(" + dimension.extent + ")");
    }
    return join(extents, " * ");
}

auto mask_index_expression(SoaMemberSchema const& member) -> std::string {
    std::vector<std::string> terms;
    auto const dimension_count{member.mask_dimensions.size()};
    terms.reserve(dimension_count);
    for (std::size_t index{}; index < dimension_count; ++index) {
        auto term{member.mask_dimensions[index].index_name};
        for (auto extent_index{index + 1}; extent_index < dimension_count; ++extent_index) {
            term += " * (" + member.mask_dimensions[extent_index].extent + ")";
        }
        terms.push_back(std::move(term));
    }
    return join(terms, " + ");
}

auto field_mask_nodes(SoaSchema const& schema, bool const standard_library = false) -> Nodes {
    if (!schema.field_mask_name.has_value()) {
        return {};
    }

    std::string const uint8_type{standard_library ? "std::uint8_t" : "uint8"};
    std::string const uint16_type{standard_library ? "std::uint16_t" : "uint16"};
    std::string const uint32_type{standard_library ? "std::uint32_t" : "uint32"};
    std::string const uint64_type{standard_library ? "std::uint64_t" : "uint64"};
    std::string const int32_type{standard_library ? "std::int32_t" : "int32"};

    auto const& mask_name{*schema.field_mask_name};
    auto const& enum_name{*schema.field_enum_name};
    std::string output{"enum class " + enum_name + " : " + uint8_type + " {\n"};
    std::string previous_name;
    std::string previous_width;
    for (auto const& member : schema.members) {
        if (!member.mask_field) {
            continue;
        }
        auto const name{title_case_identifier(member.name)};
        auto const initializer{previous_name.empty() ? "0"
                                                     : "static_cast<" + uint8_type + ">(" +
                                                           previous_name + ") + " + previous_width};
        output += "    " + name + " = " + initializer + ",\n";
        previous_name = name;
        previous_width = mask_field_width(member);
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

    for (auto const& member : schema.members) {
        if (!member.mask_field || member.mask_dimensions.empty()) {
            continue;
        }
        output += "\n    [[nodiscard]] static constexpr auto " + member.name + "_field(";
        for (std::size_t index{}; index < member.mask_dimensions.size(); ++index) {
            if (index > 0) {
                output += ", ";
            }
            output += "" + int32_type + " const " + member.mask_dimensions[index].index_name;
        }
        output += ") noexcept -> " + enum_name +
                  " {\n"
                  "        return static_cast<" +
                  enum_name + ">(static_cast<" + int32_type + ">(" + enum_name +
                  "::" + title_case_identifier(member.name) + ") + " +
                  mask_index_expression(member) +
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

    return {raw(std::move(output),
                {TypeDependency{"std::conditional_t", "type_traits", {}},
                 TypeDependency{"std::uint64_t", "cstdint", {}}})};
}

auto lower_soa_impl(SoaSchema const& schema, TypeRegistry const& types, Nodes storage_prelude)
    -> LoweredSoa {
    auto const members{resolve_members(schema, types)};
    auto const view_name{schema.view_name.value_or(schema.name + "View")};
    auto const const_view_name{schema.const_view_name.value_or(schema.name + "ConstView")};
    std::vector<FunctionSpec> custom_source;
    auto storage{soa_storage_node(schema,
                                  members,
                                  view_name,
                                  const_view_name,
                                  types,
                                  custom_source,
                                  std::move(storage_prelude))};

    NodeListBuilder header;
    auto mask_nodes{field_mask_nodes(schema)};
    if (!mask_nodes.empty()) {
        header.append(std::move(mask_nodes)).new_lines(2);
    }
    header.append(soa_view_struct_nodes(schema, members, types, view_name, const_view_name))
        .add(std::move(storage));

    NodeListBuilder source;
    bool has_source_definition{};
    auto add_definition = [&](FunctionSpec const& spec, std::string const& owner) {
        if (has_source_definition) {
            source.new_lines(2);
        }
        source.add(definition(spec, owner));
        has_source_definition = true;
    };
    for (auto const& spec : custom_source) {
        add_definition(spec, schema.name);
    }
    auto append_definitions = [&](std::vector<FunctionSpec> const& specs,
                                  std::string const& owner) {
        for (auto const& spec : specs) {
            add_definition(spec, owner);
        }
    };
    append_definitions(soa_view_specs(members, true), const_view_name);
    append_definitions(soa_view_specs(members, false), view_name);
    for (auto const& spec : soa_storage_operation_specs(schema, members)) {
        if (!spec.is_inline) {
            add_definition(spec, schema.name);
        }
    }
    auto const permutations{soa_permutation_specs(members)};
    add_definition(permutations.front(), schema.name);
    append_definitions(soa_view_specs(members, false), schema.name);
    return LoweredSoa{header.build(), source.build()};
}

auto lower_one_soa(SoaSchema const& schema,
                   std::map<std::string, SoaSchema const*> const& schemas,
                   SoaBackend const backend,
                   TypeRegistry const& types,
                   lispb::schema::TypeGraph const& type_graph,
                   std::string const& module_name) -> LoweredSoa {
    if (schema.layout_only) {
        return {};
    }
    auto const standard_library{backend == SoaBackend::standard_library};
    LoweredSoa lowered;
    if (schema.emits_vector_storage()) {
        lowered = standard_library ? lower_native_soa(schema, schemas, types)
                                   : lower_soa_impl(schema, types, {});
    }
    if (standard_library && schema.field_mask_name.has_value()) {
        NodeListBuilder header;
        header.append(field_mask_nodes(schema, true))
            .new_lines(2)
            .append(std::move(lowered.header));
        lowered.header = header.build();
    }
    if (schema.fixed.has_value()) {
        NodeListBuilder header;
        header.append(std::move(lowered.header))
            .new_lines(2)
            .append(lower_fixed_nodes(schema, schemas, types));
        lowered.header = header.build();
    }
    if (schema.single_allocation.has_value()) {
        NodeListBuilder header;
        header.append(std::move(lowered.header))
            .new_lines(2)
            .append(lower_single_allocation_nodes(
                schema, schemas, types, type_graph, module_name, backend));
        for (auto const& variant : schema.single_allocation_variants) {
            auto copy{schema};
            copy.single_allocation = variant.name;
            copy.single_allocation_allocator = variant.allocator;
            header.new_lines(2).append(lower_single_allocation_nodes(
                copy, schemas, types, type_graph, module_name, backend));
        }
        lowered.header = header.build();
    }
    return lowered;
}

} // namespace

auto lower_soa(SoaSchema const& schema, TypeRegistry const& types, Nodes storage_prelude)
    -> LoweredSoa {
    return lower_soa_impl(schema, types, std::move(storage_prelude));
}

auto lower_soa_declaration(SoaSchema const& schema,
                           NormalModuleSchema const& module,
                           TypeRegistry const& types,
                           lispb::schema::TypeGraph const& type_graph,
                           std::optional<std::string> const& allocator_prefix)
    -> DeclarationEmission {
    if (allocator_prefix.has_value() && !schema.emits_vector_storage()) {
        return {};
    }
    std::vector<SoaSchema> original_schemas;
    for (auto const& declaration : module.declarations) {
        if (auto const* soa{std::get_if<SoaSchema>(&declaration)}) {
            original_schemas.push_back(*soa);
        }
    }
    auto const schemas{expand_soa_allocator_variants(
        module.soa_backend, original_schemas, module.soa_array_allocators)};
    std::map<std::string, SoaSchema const*> by_name;
    for (auto const& item : schemas) {
        by_name.emplace(item.name, &item);
    }

    auto const wanted{allocator_prefix.has_value() ? *allocator_prefix + schema.name : schema.name};
    auto const& item{*by_name.at(wanted)};
    auto lowered{
        lower_one_soa(item, by_name, module.soa_backend, types, type_graph, module.settings.name)};
    return {.header = std::move(lowered.header),
            .source = std::move(lowered.source),
            .format_generated = module.soa_backend == SoaBackend::standard_library ||
                                item.single_allocation.has_value()};
}

} // namespace codegen::detail
