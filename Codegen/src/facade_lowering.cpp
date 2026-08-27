#include "lowering.h"
#include "lowering_utils.h"

#include <stdexcept>
#include <utility>

namespace codegen::detail {
namespace {

auto lower_facade_module_impl(FacadeModuleSchema const& module,
                              std::map<std::string, CppType> const& types) -> Module {
    auto const& facade{module.facade};
    auto target_type{resolve_type(facade.target_type, types)};
    auto const definitions_in_source{facade.definitions_in_source};
    auto bind{FunctionSpec{
        .name = "bind",
        .return_type = "void",
        .parameters = {FunctionParameter{qualify(target_type, "&"), "new_target"}},
        .body = {raw(facade.target_member_name + " = &new_target;")},
        .is_inline = !definitions_in_source,
    }};
    std::vector<FunctionSpec> methods;
    for (auto const& method : facade.methods) {
        std::vector<FunctionParameter> parameters;
        std::vector<std::string> arguments;
        for (auto const& parameter : method.parameters) {
            auto type{resolve_type(parameter.type, types)};
            if (parameter.default_value.has_value()) {
                parameters.emplace_back(std::move(type), parameter.name, *parameter.default_value);
            } else {
                parameters.emplace_back(std::move(type), parameter.name);
            }
            arguments.push_back(parameter.name);
        }
        std::vector<TypeDependency> validation_dependencies;
        for (auto const& key : facade.validation_dependencies) {
            validation_dependencies.push_back(dependency_for_key(key, types));
        }
        std::vector<std::string> body{facade.validation_lines};
        auto call{facade.target_member_name + "->" + method.target_name.value_or(method.name) +
                  "(" + join(arguments, ", ") + ");"};
        auto const return_type{resolve_type(method.return_type, types)};
        if (return_type.spelling != "void") {
            call = "return " + call;
        }
        body.push_back(std::move(call));
        methods.push_back(FunctionSpec{
            .name = method.name,
            .return_type = return_type,
            .parameters = std::move(parameters),
            .body = {raw(join_lines(body), std::move(validation_dependencies))},
            .suffix = method.suffix,
            .is_inline = !definitions_in_source,
        });
    }

    NodeListBuilder public_nodes;
    NodeListBuilder private_nodes;
    bool has_public_nodes{};
    auto add_method = [&](std::string const& access, FunctionSpec const& spec) {
        auto& destination{access == "public" ? public_nodes : private_nodes};
        destination.add(spec.is_inline ? header_function(spec) : declaration(spec), 2);
        has_public_nodes = has_public_nodes || access == "public";
    };
    add_method(facade.bind_access, bind);
    for (auto const& method : methods) {
        add_method(facade.method_access, method);
    }
    for (auto const& friend_name : facade.friends) {
        private_nodes.add(FriendDeclaration{friend_name}, 1);
    }
    private_nodes.add(Member{qualify(target_type, "*"), facade.target_member_name, "nullptr"});

    NodeListBuilder class_nodes;
    if (has_public_nodes) {
        class_nodes
            .add(AccessSpecifier{"public", AccessSpecifier::Indentation::normal}, 1)
            .append(public_nodes.build());
    }
    class_nodes.add(AccessSpecifier{"private", AccessSpecifier::Indentation::normal}, 1)
        .append(private_nodes.build());

    Node declaration_node{Struct{
        .name = facade.name,
        .children = class_nodes.build(),
        .export_specifier = facade.export_specifier,
        .record_kind = "class",
    }};
    if (module.settings.namespace_name.has_value()) {
        declaration_node = Namespace{*module.settings.namespace_name, {std::move(declaration_node)}};
    }
    NodeListBuilder header_nodes;
    header_nodes.add(IncludeDependencies{}, 2).add(std::move(declaration_node));
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
        NodeListBuilder definitions;
        definitions.add(definition(bind, facade.name));
        for (auto const& method : methods) {
            definitions.new_lines(2).add(definition(method, facade.name));
        }
        auto definition_nodes{definitions.build()};
        if (module.settings.namespace_name.has_value()) {
            definition_nodes = {
                Namespace{*module.settings.namespace_name, std::move(definition_nodes)}};
        }
        NodeListBuilder source_nodes;
        source_nodes.add(Include{source_include(module.settings), false}, 2)
            .add(IncludeDependencies{}, 2)
            .append(std::move(definition_nodes));
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

auto lower_facade_module(FacadeModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module {
    return lower_facade_module_impl(module, types);
}

} // namespace codegen::detail
