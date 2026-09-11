#include "lowering.h"
#include "lowering_utils.h"
#include "soa_internal.h"

#include <algorithm>
#include <cctype>
#include <set>
#include <stdexcept>
#include <utility>

namespace codegen::detail {
namespace {

auto lower_soa_impl(SoaSchema const& schema,
                    std::map<std::string, CppType> const& types,
                    Nodes storage_prelude) -> LoweredSoa {
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

auto lower_soa_module_impl(SoaModuleSchema const& module,
                           std::map<std::string, CppType> const& types) -> Module {
    auto const format_generated{module.experimental_stdlib ||
                                std::ranges::any_of(module.structs, [](auto const& schema) {
                                    return schema.single_allocation.has_value();
                                })};
    std::map<std::string, SoaSchema const*> schemas;
    for (auto const& schema : module.structs) {
        schemas.emplace(schema.name, &schema);
    }

    std::vector<LoweredSoa> lowered_structs;
    lowered_structs.reserve(module.structs.size());
    for (auto const& schema : module.structs) {
        auto lowered{module.experimental_stdlib ? lower_native_soa(schema, schemas, types)
                                                : lower_soa_impl(schema, types, {})};
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
                    schema, schemas, types, module.experimental_stdlib));
            for (auto const& variant : schema.single_allocation_variants) {
                auto copy{schema};
                copy.single_allocation = variant.name;
                copy.single_allocation_allocator = variant.allocator;
                header.new_lines(2).append(lower_single_allocation_nodes(
                    copy, schemas, types, module.experimental_stdlib));
            }
            lowered.header = header.build();
        }
        lowered_structs.push_back(std::move(lowered));
    }

    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2);
    if (!module.settings.prelude_lines.empty()) {
        header_nodes.add(raw(join_lines(module.settings.prelude_lines)), 2);
    }
    NodeListBuilder definitions;
    for (std::size_t index{0}; index < lowered_structs.size(); ++index) {
        if (index > 0) {
            definitions.new_lines(2);
        }
        definitions.append(std::move(lowered_structs[index].header));
    }
    auto definition_nodes{definitions.build()};
    if (module.settings.namespace_name.has_value()) {
        header_nodes.add(Namespace{*module.settings.namespace_name, std::move(definition_nodes)});
    } else {
        header_nodes.append(std::move(definition_nodes));
    }
    Module result{
        .name = module.settings.name,
        .header =
            CppFile{
                .path = module.settings.header,
                .nodes = header_nodes.build(),
                .clang_format_off = !format_generated,
                .include_order = module.settings.include_order,
                .format_generated = format_generated,
            },
    };
    if (module.settings.source.has_value()) {
        NodeListBuilder source_definitions;
        for (std::size_t index{0}; index < lowered_structs.size(); ++index) {
            if (index > 0) {
                source_definitions.new_lines(2);
            }
            source_definitions.append(std::move(lowered_structs[index].source));
        }
        auto source_definition_nodes{source_definitions.build()};
        if (module.settings.namespace_name.has_value()) {
            source_definition_nodes = {
                Namespace{*module.settings.namespace_name, std::move(source_definition_nodes)}};
        }
        NodeListBuilder source_nodes;
        source_nodes.add(Include{source_include(module.settings), false}, 2)
            .add(IncludeDependencies{}, 2)
            .append(std::move(source_definition_nodes));
        result.source = CppFile{
            .path = *module.settings.source,
            .nodes = source_nodes.build(),
            .pragma_once = false,
            .clang_format_off = !format_generated,
            .include_order = module.settings.include_order,
            .format_generated = format_generated,
        };
    }
    return result;
}

} // namespace

auto lower_soa(SoaSchema const& schema,
               std::map<std::string, CppType> const& types,
               Nodes storage_prelude) -> LoweredSoa {
    return lower_soa_impl(schema, types, std::move(storage_prelude));
}

auto lower_soa_module(SoaModuleSchema const& module, std::map<std::string, CppType> const& types)
    -> Module {
    if (module.experimental_array_allocators.empty()) {
        return lower_soa_module_impl(module, types);
    }
    if (module.experimental_stdlib) {
        throw std::invalid_argument{"TArray allocator variants require the Unreal backend"};
    }
    auto expanded{module};
    std::set<std::string> names;
    for (auto const& schema : module.structs) {
        names.insert(schema.name);
        names.insert(schema.view_name.value_or(schema.name + "View"));
        names.insert(schema.const_view_name.value_or(schema.name + "ConstView"));
        if (schema.single_allocation) {
            names.insert(*schema.single_allocation);
            names.insert(*schema.single_allocation + "Storage");
        }
        for (auto const& variant : schema.single_allocation_variants) {
            names.insert(variant.name);
            names.insert(variant.name + "Storage");
        }
    }
    for (auto const& variant : module.experimental_array_allocators) {
        if (variant.prefix.empty() ||
            !std::isalpha(static_cast<unsigned char>(variant.prefix.front())) ||
            !std::ranges::all_of(variant.prefix,
                                 [](unsigned char c) { return std::isalnum(c) || c == '_'; })) {
            throw std::invalid_argument{"Invalid SoA allocator variant prefix: " + variant.prefix};
        }
        for (auto const& schema : module.structs) {
            if (schema.fixed || schema.equivalent_type || !schema.functions.empty() ||
                !schema.mutable_view_functions.empty() || !schema.using_declarations.empty()) {
                throw std::invalid_argument{"SoA allocator variants require plain dynamic schemas"};
            }
            auto copy{schema};
            copy.name = variant.prefix + schema.name;
            copy.view_name = variant.prefix + schema.view_name.value_or(schema.name + "View");
            copy.const_view_name =
                variant.prefix + schema.const_view_name.value_or(schema.name + "ConstView");
            for (auto const& name : {copy.name, *copy.view_name, *copy.const_view_name}) {
                if (!names.insert(name).second) {
                    throw std::invalid_argument{"Duplicate SoA allocator variant type: " + name};
                }
            }
            copy.single_allocation.reset();
            copy.single_allocation_variants.clear();
            copy.array_allocator = variant.allocator;
            for (auto& member : copy.members) {
                if (member.kind == SoaMemberKind::nested) {
                    if (!member.nested_schema) {
                        throw std::invalid_argument{
                            "SoA allocator variants require generated nested schemas"};
                    }
                    member.type = TypeRef{variant.prefix + *member.nested_schema};
                    member.nested_schema = variant.prefix + *member.nested_schema;
                }
            }
            expanded.structs.push_back(std::move(copy));
        }
    }
    return lower_soa_module_impl(expanded, types);
}

} // namespace codegen::detail
