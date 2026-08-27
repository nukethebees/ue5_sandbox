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

    Nodes public_nodes;
    Nodes private_nodes;
    auto add_method = [&](std::string const& access, FunctionSpec const& spec) {
        auto& destination{access == "public" ? public_nodes : private_nodes};
        destination.push_back(spec.is_inline ? header_function(spec) : declaration(spec));
        destination.push_back(lines(2));
    };
    add_method(facade.bind_access, bind);
    for (auto const& method : methods) {
        add_method(facade.method_access, method);
    }
    for (auto const& friend_name : facade.friends) {
        private_nodes.push_back(raw("friend class " + friend_name + ";"));
        private_nodes.push_back(lines(1));
    }
    private_nodes.push_back(Member{qualify(target_type, "*"), facade.target_member_name, "nullptr"});

    Nodes class_nodes;
    if (!public_nodes.empty()) {
        if (public_nodes.back().is<NewLines>()) {
            public_nodes.pop_back();
        }
        class_nodes.push_back(raw("public:"));
        class_nodes.push_back(lines(1));
        class_nodes.insert(class_nodes.end(), public_nodes.begin(), public_nodes.end());
        class_nodes.push_back(lines(2));
    }
    if (private_nodes.back().is<NewLines>()) {
        private_nodes.pop_back();
    }
    class_nodes.push_back(raw("private:"));
    class_nodes.push_back(lines(1));
    class_nodes.insert(class_nodes.end(), private_nodes.begin(), private_nodes.end());

    Nodes header_nodes{
        IncludeDependencies{},
        lines(2),
        Struct{
            .name = facade.name,
            .children = std::move(class_nodes),
            .export_specifier = facade.export_specifier,
            .record_kind = "class",
        },
    };
    if (module.settings.namespace_name.has_value()) {
        auto declaration_node{std::move(header_nodes.back())};
        header_nodes.pop_back();
        header_nodes.push_back(
            Namespace{*module.settings.namespace_name, {std::move(declaration_node)}});
    }
    Module result{
        .name = module.settings.name,
        .header = CppFile{
            .path = module.settings.header,
            .nodes = std::move(header_nodes),
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        },
    };
    if (module.settings.source.has_value()) {
        Nodes definitions{definition(bind, facade.name), lines(2)};
        for (std::size_t index{0}; index < methods.size(); ++index) {
            definitions.push_back(definition(methods[index], facade.name));
            if (index + 1 < methods.size()) {
                definitions.push_back(lines(2));
            }
        }
        if (module.settings.namespace_name.has_value()) {
            definitions = {Namespace{*module.settings.namespace_name, std::move(definitions)}};
        }
        result.source = CppFile{
            .path = *module.settings.source,
            .nodes = {Include{source_include(module.settings), false},
                      lines(2),
                      IncludeDependencies{},
                      lines(2)},
            .pragma_once = false,
            .clang_format_off = true,
            .include_order = module.settings.include_order,
        };
        result.source->nodes.insert(result.source->nodes.end(),
                                    definitions.begin(),
                                    definitions.end());
    }
    return result;
}

} // namespace

auto lower_facade_module(FacadeModuleSchema const& module,
                         std::map<std::string, CppType> const& types) -> Module {
    return lower_facade_module_impl(module, types);
}

} // namespace codegen::detail
