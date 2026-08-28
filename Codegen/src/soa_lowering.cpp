#include "lowering.h"
#include "lowering_utils.h"
#include "soa_internal.h"

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
    auto storage{soa_storage_node(
        schema, members, view_name, const_view_name, types, custom_source, std::move(storage_prelude))};

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
    std::map<std::string, SoaSchema const*> schemas;
    for (auto const& schema : module.structs) {
        schemas.emplace(schema.name, &schema);
    }

    std::vector<LoweredSoa> lowered_structs;
    lowered_structs.reserve(module.structs.size());
    for (auto const& schema : module.structs) {
        auto lowered{lower_soa_impl(schema, types, {})};
        if (schema.fixed.has_value()) {
            NodeListBuilder header;
            header.append(std::move(lowered.header))
                .new_lines(2)
                .append(lower_fixed_nodes(schema, schemas, types));
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
        .header = CppFile{
            .path = module.settings.header,
            .nodes = header_nodes.build(),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
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
            .clang_format_off = true,
            .include_order = module.settings.include_order,
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

auto lower_soa_module(SoaModuleSchema const& module,
                      std::map<std::string, CppType> const& types) -> Module {
    return lower_soa_module_impl(module, types);
}

} // namespace codegen::detail
