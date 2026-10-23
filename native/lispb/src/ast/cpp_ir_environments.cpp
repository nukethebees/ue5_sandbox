#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/expr_kind.h>
#include <codegen/ast/node_kind.h>
#include <codegen/ast/template_kind.h>

#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen {

namespace environment_detail {
auto components(Strings& strings, std::string name) -> std::vector<IdentifierId> {
    // Own the spelling while interning components, which can move the string buffer.
    std::vector<IdentifierId> result;
    std::string_view remaining{name};
    if (remaining.starts_with("::")) {
        remaining.remove_prefix(2);
    }
    while (!remaining.empty()) {
        auto const end{remaining.find("::")};
        result.push_back(strings.intern(remaining.substr(0, end)));
        if (end == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(end + 2);
    }
    return result;
}
auto untemplated(std::string_view name) -> std::string {
    std::string result;
    unsigned depth{};
    for (auto const character : name) {
        if (character == '<') {
            ++depth;
        } else if (character == '>' && depth != 0) {
            --depth;
        } else if (depth == 0) {
            result += character;
        }
    }
    return result;
}
}

void CppIR::set_scope_bindings(NodeId node, std::span<IdentifierId const> names) {
    scope_bindings_[node.value] = {names.begin(), names.end()};
}
auto CppIR::declaration_name(Expr expression) const -> IdentifierId {
    while (expression.kind() == ExprKind::template_id) {
        expression = template_id_exprs_.callee[expression.index()];
    }
    if (expression.kind() != ExprKind::named) {
        return {};
    }
    auto const text{strings.view(named_exprs_.spelling[expression.index()])};
    return strings.intern(text.substr(0, text.find_first_of("<:")));
}
void CppIR::collect_template_parameters(RenderState& state,
                                        EnvironmentId environment,
                                        TemplateId declaration) const {
    if (!declaration || declaration.kind() == TemplateKind::explicit_specialization) {
        return;
    }
    if (declaration.kind() == TemplateKind::authored_raw) {
        state.environments.mark_unknown(environment);
        return;
    }

    auto const range{templates_.parameters[declaration.index()]};
    auto const end{range.begin + range.count};
    for (auto index{range.begin}; index < end; ++index) {
        auto const parameter{template_parameter_references_[index]};
        auto const name{parameter.kind() == TemplateParameterKind::type
                            ? type_template_parameters_.name[parameter.index()]
                            : value_template_parameters_.name[parameter.index()]};
        if (name.value != 0) {
            state.environments.declare(environment, name, CppDeclarationId{parameter});
        }
    }
}
void CppIR::collect_environment(RenderState& state,
                                EnvironmentId environment,
                                std::span<NodeId const> children,
                                bool complete) const {
    auto& environments{state.environments};
    for (auto const node : children) {
        auto const i{node.index()};
        auto add_name = [&](auto name, CppDeclarationId declaration) {
            std::string const text{strings.view(name)};
            if (!text.empty()) {
                environments.declare(environment,
                                     strings.intern(text.substr(0, text.find_first_of("<:"))),
                                     declaration);
            }
        };
        auto add_expr = [&](Expr expression, CppDeclarationId declaration) {
            auto const name{declaration_name(expression)};
            if (name.value != 0) {
                environments.declare(environment, name, declaration);
            } else {
                environments.mark_unknown(environment);
            }
        };
        auto const declaration{CppDeclarationId{node}};
        switch (node.kind()) {
            case NodeKind::namespace_: {
                std::string const name{strings.view(namespace_nodes_.name[i])};
                auto nested{environment};
                if (name.empty()) {
                    nested = environments.append(environment, EnvironmentKind::namespace_);
                } else {
                    for (auto const component : environment_detail::components(strings, name)) {
                        nested = environments.named(nested, EnvironmentKind::namespace_, component);
                    }
                }
                state.node_environments[RenderState::node_key(node, environment)] = nested;
                collect_environment(state, nested, nodes(namespace_nodes_.children[i]));
                if (name.empty()) {
                    environments.import_symbols(environment, nested);
                }
                break;
            }
            case NodeKind::struct_: {
                auto const name{declaration_name(struct_nodes_.name[i])};
                auto nested{EnvironmentId{}};
                if (name.value == 0) {
                    environments.mark_unknown(environment);
                    nested = environments.append(environment, EnvironmentKind::record);
                    environments.mark_unknown(nested);
                } else {
                    nested =
                        environments.named(environment, EnvironmentKind::record, name, declaration);
                    auto const symbol{environments.declare(environment, name, declaration, nested)};
                    environments.bind(nested, name, symbol);
                }
                state.node_environments[RenderState::node_key(node, environment)] = nested;
                collect_template_parameters(state, nested, struct_nodes_.template_declaration[i]);
                auto const known{scope_bindings_.contains(node.value)};
                collect_environment(state, nested, nodes(struct_nodes_.children[i]), known);
                if (!known && struct_nodes_.bases[i].count != 0) {
                    environments.mark_unknown(nested);
                }
                if (known) {
                    for (auto const binding : scope_bindings_.at(node.value)) {
                        environments.declare(nested, binding, declaration);
                    }
                }
                break;
            }
            case NodeKind::enum_: {
                auto const name{strings.intern(strings.view(enum_nodes_.name[i]))};
                auto const nested{
                    environments.named(environment, EnvironmentKind::record, name, declaration)};
                auto const range{enum_nodes_.values[i]};
                auto const end{range.begin + range.count};
                for (auto index{range.begin}; index < end; ++index) {
                    auto const enumerator{enumerator_references_[index]};
                    environments.declare(
                        nested, strings.intern(enumerators_.name[enumerator.value]), declaration);
                }
                if (!enum_nodes_.scoped[i]) {
                    environments.import_symbols(environment, nested);
                }
                break;
            }
            case NodeKind::forward_declaration:
                add_name(forward_declaration_nodes_.name[i], declaration);
                break;
            case NodeKind::using_declaration:
                add_name(using_declaration_nodes_.name[i], declaration);
                break;
            case NodeKind::instance_member:
                add_name(instance_members_.name[i], declaration);
                break;
            case NodeKind::static_member:
                add_name(static_members_.name[i], declaration);
                break;
            case NodeKind::constexpr_member:
                add_name(constexpr_members_.name[i], declaration);
                break;
            case NodeKind::function:
                if (!function_nodes_.owner[i]) {
                    add_expr(functions_.name[function_nodes_.function[i].value],
                             CppDeclarationId{function_nodes_.function[i]});
                }
                break;
            case NodeKind::raw:
            case NodeKind::raw_lines:
                if (!complete && !scope_bindings_.contains(node.value)) {
                    auto const text{node.kind() == NodeKind::raw ? strings.view(raw_nodes_.text[i])
                                                                 : std::string_view{}};
                    if ((!text.empty() && !text.starts_with("#include ")) ||
                        node.kind() == NodeKind::raw_lines) {
                        environments.mark_unknown(environment);
                    }
                }
                break;
            default:
                break;
        }
        if (node.kind() != NodeKind::struct_) {
            if (auto const extra{scope_bindings_.find(node.value)};
                extra != scope_bindings_.end()) {
                for (auto const name : extra->second) {
                    add_name(name, declaration);
                }
            }
        }
    }
}
auto CppIR::block_environment(RenderState& state,
                              std::span<NodeId const> children,
                              EnvironmentKind kind) const -> EnvironmentId {
    auto const environment{state.environments.append(state.current, kind)};
    collect_environment(state, environment, children);
    return environment;
}
auto CppIR::record_environment(std::string_view owner, RenderState& state) const -> EnvironmentId {
    auto const name{environment_detail::untemplated(owner)};
    auto const components{environment_detail::components(strings, name)};
    auto const parent{name.starts_with("::") ? state.environments.global() : state.current};
    auto const found{state.environments.find_environment(parent, components)};
    if (!found.is_null()) {
        return found;
    }

    auto const unknown{state.environments.append(state.current, EnvironmentKind::record)};
    state.environments.mark_unknown(unknown);
    return unknown;
}
void CppIR::initialize_environments(RenderState& state, std::span<NodeId const> children) const {
    collect_environment(state, state.environments.global(), children);
}

}
