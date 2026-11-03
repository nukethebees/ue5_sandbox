#include <codegen/ast/cpp_renderer.h>

#include <ioj/is_any.h>

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/environment_guard.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/expr_kind.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_disposition_placement.h>
#include <codegen/ast/function_exception_id.h>
#include <codegen/ast/function_exception_kind.h>
#include <codegen/ast/function_implementation_id.h>
#include <codegen/ast/function_implementation_kind.h>
#include <codegen/ast/function_kind.h>
#include <codegen/ast/function_opening_brace_placement.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_requires_placement.h>
#include <codegen/ast/function_template_placement.h>
#include <codegen/ast/function_trailing_return_placement.h>
#include <codegen/ast/include.h>
#include <codegen/ast/include_style.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/ir/range_storage.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_kind.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/template_kind.h>
#include <codegen/ast/template_parameter_kind.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/cpp_string.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <iterator>
#include <limits>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {

namespace environment_detail {
auto components(Strings& strings, std::string const& name) -> std::vector<StringId> {
    // Borrow an owned spelling because interning can move the Strings buffer.
    std::vector<StringId> result;
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

namespace expr_detail {
template <typename Value>
void append_numeric_literal(std::string& output, Value const value) {
    auto spelling{std::format("{}", value)};
    if constexpr (std::floating_point<Value>) {
        if (spelling.find_first_of(".eE") == std::string::npos) {
            spelling += ".0";
        }
    }
    output += spelling;
    if constexpr (std::same_as<Value, unsigned int>) {
        output += 'U';
    } else if constexpr (std::same_as<Value, long>) {
        output += 'L';
    } else if constexpr (std::same_as<Value, unsigned long>) {
        output += "UL";
    } else if constexpr (std::same_as<Value, long long>) {
        output += "LL";
    } else if constexpr (std::same_as<Value, unsigned long long>) {
        output += "ULL";
    } else if constexpr (std::same_as<Value, float>) {
        output += 'f';
    }
}

auto binary_precedence(BinaryOperator operation) -> int {
    switch (operation) {
        case BinaryOperator::logical_or:
            return 1;
        case BinaryOperator::logical_and:
            return 2;
        case BinaryOperator::bitwise_or:
            return 3;
        case BinaryOperator::bitwise_and:
            return 4;
        case BinaryOperator::equal:
        case BinaryOperator::not_equal:
            return 5;
        case BinaryOperator::greater_equal:
        case BinaryOperator::greater:
        case BinaryOperator::less:
        case BinaryOperator::less_equal:
            return 6;
        case BinaryOperator::three_way:
            return 7;
        case BinaryOperator::shift_left:
            return 8;
        case BinaryOperator::subtract:
        case BinaryOperator::add:
            return 9;
        case BinaryOperator::multiply:
        case BinaryOperator::divide:
            return 10;
    }
    std::unreachable();
}
}

/* **************************************** */
// Entry points
/* **************************************** */
CppRenderer::CppRenderer(CppIR const& ir, std::string_view indent_text)
    : ir_{ir}
    , indent_text_{indent_text} {}
auto CppRenderer::render(Expr expr, RenderContext const& ctx) -> std::string {
    state_.clear();
    return expression(expr, ctx);
}
auto CppRenderer::render(NodeId node, RenderContext const& ctx) -> std::string {
    state_.clear();
    initialize_environments(std::span{&node, 1});
    std::string output;
    append_node(output, node, ctx);
    return output;
}
auto CppRenderer::render(CppFileId file) -> std::string {
    state_.clear();
    auto const row{ir_.files.index(file)};
    auto const file_nodes{ir_.files.nodes(file)};
    auto const flags{ir_.files.flags[row]};
    std::vector<TypeDependencyId> dependencies;
    for (auto const node : file_nodes) {
        ir_.collect_dependencies(node, dependencies);
    }
    std::set<std::string> paths;
    for (auto const dependency : dependencies) {
        if (auto const header{ir_.types.dependency_header(dependency)}) {
            paths.emplace(*header);
        }
    }
    std::vector<std::vector<std::string>> include_groups;
    for (auto const prefix_id : ir_.files.include_order(file)) {
        auto const prefix{ir_.strings.view(prefix_id)};
        std::vector<std::string> group;
        for (auto it{paths.begin()}; it != paths.end();) {
            if (it->starts_with(prefix)) {
                group.push_back(*it);
                it = paths.erase(it);
            } else {
                ++it;
            }
        }
        if (!group.empty()) {
            include_groups.push_back(std::move(group));
        }
    }
    std::vector<std::string> quoted;
    std::vector<std::string> system;
    for (auto const& path : paths) {
        (include_is_system(path) ? system : quoted).push_back(path);
    }
    if (!quoted.empty()) {
        include_groups.push_back(std::move(quoted));
    }
    if (!system.empty()) {
        include_groups.push_back(std::move(system));
    }
    initialize_environments(file_nodes);
    RenderContext const ctx{.include_groups = include_groups};
    std::string output;
    if (flags.clang_format_off()) {
        output += "// clang-format off\n";
    }
    output += "// This file is autogenerated. Do not edit by hand.\n";
    if (ir_.files.source[row].value != 0) {
        output += "// Source: ";
        output += ir_.strings.view(ir_.files.source[row]);
    } else {
        output += "// Edit lispb/schema and regenerate it instead.";
    }
    for (auto const prologue : ir_.files.prologue(file)) {
        output += "\n\n";
        output += ir_.strings.view(prologue);
    }
    if (flags.pragma_once()) {
        output += "\n\n#pragma once";
    }
    if (!file_nodes.empty()) {
        output += "\n\n";
        append_nodes(output, file_nodes, ctx, 1);
    }
    for (auto const epilogue : ir_.files.epilogue(file)) {
        output += "\n\n";
        output += ir_.strings.view(epilogue);
    }
    while (!output.empty() && output.back() == '\n') {
        output.pop_back();
    }
    output += '\n';
    if (flags.clang_format_off()) {
        output += "// clang-format on\n";
    }
    return output;
}
auto CppRenderer::render_nodes(std::span<NodeId const> values,
                               RenderContext const& ctx,
                               int newlines) -> std::string {
    state_.clear();
    initialize_environments(values);
    std::string output;
    append_nodes(output, values, ctx, newlines);
    return output;
}

/* **************************************** */
// Formatting
/* **************************************** */
void CppRenderer::append_indent(std::string& output, RenderContext const& ctx) const {
    for (int level{}; level < ctx.indent_level; ++level) {
        output += indent_text_;
    }
}
void CppRenderer::append_indented(std::string& output,
                                  std::string_view text,
                                  RenderContext const& ctx) const {
    bool line_start{true};
    for (auto const character : text) {
        if (line_start && character != '\n') {
            append_indent(output, ctx);
        }
        output += character;
        line_start = character == '\n';
    }
}

/* **************************************** */
// Types
auto CppRenderer::in_record_scope(EnvironmentId environment) const -> bool {
    while (state_.environments.kind(environment) == EnvironmentKind::template_) {
        environment = state_.environments.parent(environment);
    }
    return state_.environments.kind(environment) == EnvironmentKind::record;
}

/* **************************************** */
// Types and environments
/* **************************************** */
void CppRenderer::append_type(std::string& output, CppTypeId const type, RenderContext const& ctx) {
    assert(type.value != 0 && type.value <= ir_.types.shapes_.size());
    auto const shape{ir_.types.shapes_[type.value - 1]};
    if (shape.form == CppTypeForm::closure) {
        throw std::logic_error{
            "An unnamed closure type must be named through its owning declaration"};
    }

    if (shape.form == CppTypeForm::raw) {
        output += ir_.types.spelling(type);
    } else if (shape.form == CppTypeForm::named) {
        std::vector<StringId> components;
        for (auto const component :
             std::span{ir_.types.name_components_}.subspan(shape.parts.begin, shape.parts.count)) {
            components.push_back(component);
        }
        append_name(output, components, shape.origin);
    } else if (shape.form == CppTypeForm::decltype_) {
        output += "decltype(";
        append_expr(output, shape.operand, ctx);
        output += ')';
    } else if (shape.form == CppTypeForm::template_id) {
        append_expr(output, shape.operand, ctx);
    } else if (shape.form == CppTypeForm::nested) {
        append_type(output, shape.owner, ctx);
        output += "::";
        output += ir_.strings.view(shape.member);
    } else {
        for (auto const part :
             std::span{ir_.types.parts_}.subspan(shape.parts.begin, shape.parts.count)) {
            if (part.type.value != 0) {
                append_type(output, part.type, ctx);
            } else {
                output += ir_.strings.view(part.text);
            }
        }
    }
}
auto CppRenderer::alias_scope(SymbolBinding alias) -> EnvironmentId {
    auto const shape{ir_.types.shapes_[alias.aliased_type.value - 1]};
    if (shape.form != CppTypeForm::named) {
        return {};
    }
    auto const components{
        std::span{ir_.types.name_components_}.subspan(shape.parts.begin, shape.parts.count)};
    auto const resolved{resolve_name(components, shape.origin, alias.declaring_scope)};
    if (resolved.prefix_size != components.size()) {
        return {};
    }
    return resolved.symbol.aliased_type.value != 0 ? alias_scope(resolved.symbol)
                                                   : resolved.symbol.introduced_scope;
}

auto CppRenderer::resolve_name(std::span<StringId const> components,
                               NameOrigin origin,
                               EnvironmentId environment) -> ResolvedName {
    auto& environments{state_.environments};
    auto const start{origin == NameOrigin::global ? environments.global() : environment};
    auto const first{environments.lookup(start, components.first(1))};
    if (first.status != LookupStatus::found) {
        return origin == NameOrigin::global
                 ? ResolvedName{environments.reference(start, components), components.size()}
                 : ResolvedName{};
    }

    ResolvedName result{first.symbol, 1};
    auto const count{components.size()};
    while (result.prefix_size < count) {
        auto scope{result.symbol.introduced_scope};
        if (result.symbol.aliased_type.value != 0) {
            scope = alias_scope(result.symbol);
        }
        if (scope.is_null()) {
            return result;
        }
        auto const next{environments.lookup_local(scope, components[result.prefix_size])};
        if (next.status != LookupStatus::found) {
            if (origin == NameOrigin::global) {
                return {environments.reference(scope, components.subspan(result.prefix_size)),
                        count};
            }
            return result;
        }
        result.symbol = next.symbol;
        ++result.prefix_size;
    }
    return result;
}

void CppRenderer::append_name(std::string& output,
                              std::span<StringId const> components,
                              NameOrigin origin) {
    std::string canonical;
    for (auto const component : components) {
        if (!canonical.empty()) {
            canonical += "::";
        }
        canonical += ir_.strings.view(component);
    }
    auto const target{resolve_name(components, origin, state_.current)};
    if (target.prefix_size == 0) {
        output += canonical;
        return;
    }

    auto const count{components.size()};
    for (auto first{target.prefix_size - 1};; --first) {
        auto const candidate{components.subspan(first, target.prefix_size - first)};
        auto const result{resolve_name(candidate, NameOrigin::enclosing, state_.current)};
        if (result.prefix_size == candidate.size() &&
            result.symbol.same_declaration(target.symbol)) {
            for (auto component{first}; component < count; ++component) {
                if (component != first) {
                    output += "::";
                }
                output += ir_.strings.view(components[component]);
            }
            return;
        }
        if (first == 0) {
            // Preserve authored qualification when the complete name cannot be checked.
            if (origin == NameOrigin::global &&
                state_.environments.lookup(state_.current, components.first(1)).status !=
                    LookupStatus::unknown) {
                output += "::";
            }
            output += canonical;
            return;
        }
    }
}

/* **************************************** */
// Environments, expressions and nodes
auto CppRenderer::declaration_name(Syntax syntax) -> StringId {
    while (syntax.kind() == SyntaxKind::template_id) {
        syntax = ir_.template_id_exprs_.callee[syntax.index()];
    }
    if (syntax.kind() != SyntaxKind::name) {
        return {};
    }
    auto const text{ir_.strings.view(ir_.named_exprs_.spelling[syntax.index()])};
    return ir_.strings.intern(text.substr(0, text.find_first_of("<:")));
}
void CppRenderer::declare_name(EnvironmentId environment,
                               Syntax syntax,
                               CppDeclarationId declaration) {
    auto& environments{state_.environments};
    if (syntax.kind() == SyntaxKind::binding_list) {
        for (auto const name :
             ir::range_values(ir_.binding_names_, ir_.binding_lists_[syntax.index()])) {
            environments.declare(environment, name, declaration);
        }
        return;
    }
    auto const name{declaration_name(syntax)};
    if (name.value != 0) {
        environments.declare(environment, name, declaration);
    } else {
        environments.mark_unknown(environment);
    }
}
void CppRenderer::collect_environment(EnvironmentId environment,
                                      std::span<NodeId const> children,
                                      bool bindings_known) {
    auto& state{state_};
    auto& environments{state.environments};
    for (auto const node : children) {
        auto const i{node.index()};
        auto add_name = [&](auto name, CppDeclarationId declaration) {
            std::string const text{ir_.strings.view(name)};
            if (!text.empty()) {
                environments.declare(environment,
                                     ir_.strings.intern(text.substr(0, text.find_first_of("<:"))),
                                     declaration);
            }
        };
        auto const declaration{CppDeclarationId{node}};
        switch (node.kind()) {
            case NodeKind::namespace_: {
                std::string const name{ir_.strings.view(ir_.namespace_nodes_.name[i])};
                auto nested{environment};
                if (name.empty()) {
                    nested = environments.append(environment, EnvironmentKind::namespace_);
                } else {
                    for (auto const component : environment_detail::components(ir_.strings, name)) {
                        nested = environments.named(nested, EnvironmentKind::namespace_, component);
                    }
                }
                state.node_environments[RenderState::node_key(node, environment)] = nested;
                collect_environment(nested, ir_.nodes(ir_.namespace_nodes_.children[i]));
                if (name.empty()) {
                    environments.import_symbols(environment, nested);
                }
                break;
            }
            case NodeKind::struct_: {
                auto const name{declaration_name(ir_.struct_nodes_.name[i])};
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
                collect_template_parameters(nested, ir_.struct_nodes_.template_declaration[i]);
                auto const known{ir_.scope_bindings_.contains(node.value)};
                collect_environment(nested, ir_.nodes(ir_.struct_nodes_.children[i]), known);
                if (!known && ir_.struct_nodes_.bases[i].count != 0) {
                    environments.mark_unknown(nested);
                }
                if (known) {
                    for (auto const binding : ir_.scope_bindings_.at(node.value)) {
                        environments.declare(nested, binding, declaration);
                    }
                }
                break;
            }
            case NodeKind::enum_: {
                auto const name{ir_.strings.intern(ir_.strings.view(ir_.enum_nodes_.name[i]))};
                auto const nested{
                    environments.named(environment, EnvironmentKind::record, name, declaration)};
                auto const range{ir_.enum_nodes_.values[i]};
                for (auto const enumerator : ir::range_values(ir_.enumerator_references_, range)) {
                    environments.declare(
                        nested,
                        ir_.strings.intern(ir_.enumerators_.name[enumerator.value]),
                        declaration);
                }
                if (!ir_.enum_nodes_.scoped[i]) {
                    environments.import_symbols(environment, nested);
                }
                break;
            }
            case NodeKind::forward_declaration:
                add_name(ir_.forward_declaration_nodes_.name[i], declaration);
                break;
            case NodeKind::using_declaration:
                if (ir_.using_declaration_nodes_.name[i].value != 0) {
                    environments.declare(environment,
                                         ir_.using_declaration_nodes_.name[i],
                                         declaration,
                                         {},
                                         ir_.using_declaration_nodes_.type[i]);
                } else {
                    auto const type{ir_.using_declaration_nodes_.type[i]};
                    auto const shape{ir_.types.shapes_[type.value - 1]};
                    assert(shape.form == CppTypeForm::named && shape.parts.count > 1);
                    auto const components{std::span{ir_.types.name_components_}.subspan(
                        shape.parts.begin, shape.parts.count)};
                    auto const qualifier{resolve_name(
                        components.first(components.size() - 1), shape.origin, environment)};
                    auto const inherited_constructor{
                        qualifier.prefix_size == components.size() - 1 &&
                        qualifier.symbol.aliased_type.value != 0 &&
                        components.back() == components[components.size() - 2]};
                    if (!inherited_constructor) {
                        auto const target{resolve_name(components, shape.origin, environment)};
                        if (target.prefix_size == components.size()) {
                            environments.bind(environment, components.back(), target.symbol);
                        } else {
                            add_name(components.back(), declaration);
                        }
                    }
                }
                break;
            case NodeKind::instance_member:
                add_name(ir_.instance_members_.name[i], declaration);
                break;
            case NodeKind::static_member:
                add_name(ir_.static_members_.name[i], declaration);
                break;
            case NodeKind::constexpr_member:
                add_name(ir_.constexpr_members_.name[i], declaration);
                break;
            case NodeKind::variable_declaration_stmt:
                if (ir_.variable_declaration_stmt_nodes_.template_declaration[i]) {
                    declare_name(
                        environment, ir_.variable_declaration_stmt_nodes_.name[i], declaration);
                }
                break;
            case NodeKind::function:
                if (!ir_.function_nodes_.owner[i]) {
                    declare_name(environment,
                                 ir_.get_view(ir_.function_nodes_.function[i]).name,
                                 CppDeclarationId{ir_.function_nodes_.function[i]});
                }
                break;
            case NodeKind::raw:
            case NodeKind::raw_lines:
                if (!bindings_known && !ir_.scope_bindings_.contains(node.value)) {
                    auto const text{node.kind() == NodeKind::raw
                                        ? ir_.strings.view(ir_.raw_nodes_.text[i])
                                        : std::string_view{}};
                    if ((!text.empty() && !text.starts_with("#include ")) ||
                        (node.kind() == NodeKind::raw && ir_.raw_nodes_.parts[i].count != 0) ||
                        node.kind() == NodeKind::raw_lines) {
                        environments.mark_unknown(environment);
                    }
                }
                break;
            default:
                break;
        }
        if (node.kind() != NodeKind::struct_) {
            if (auto const extra{ir_.scope_bindings_.find(node.value)};
                extra != ir_.scope_bindings_.end()) {
                for (auto const name : extra->second) {
                    add_name(name, declaration);
                }
            }
        }
    }
}
void CppRenderer::collect_template_parameters(EnvironmentId environment, TemplateId declaration) {
    auto& state{state_};
    if (!declaration || declaration.kind() == TemplateKind::explicit_specialization) {
        return;
    }
    if (declaration.kind() == TemplateKind::authored_raw) {
        state.environments.mark_unknown(environment);
        return;
    }

    auto const range{ir_.templates_.parameters[declaration.index()]};
    for (auto const parameter : ir::range_values(ir_.template_parameter_references_, range)) {
        auto const name{parameter.kind() == TemplateParameterKind::type
                            ? ir_.type_template_parameters_.name[parameter.index()]
                            : ir_.value_template_parameters_.name[parameter.index()]};
        if (name.value != 0) {
            state.environments.declare(environment, name, CppDeclarationId{parameter});
        }
    }
}
auto CppRenderer::block_environment(std::span<NodeId const> children, EnvironmentKind kind)
    -> EnvironmentId {
    auto& state{state_};
    auto const environment{state.environments.append(state.current, kind)};
    collect_environment(environment, children);
    return environment;
}
auto CppRenderer::record_environment(std::string_view owner) -> EnvironmentId {
    auto& state{state_};
    auto const name{environment_detail::untemplated(owner)};
    auto const components{environment_detail::components(ir_.strings, name)};
    auto const parent{name.starts_with("::") ? state.environments.global() : state.current};
    auto const found{state.environments.find_environment(parent, components)};
    if (!found.is_null()) {
        return found;
    }

    auto const unknown{state.environments.append(state.current, EnvironmentKind::record)};
    state.environments.mark_unknown(unknown);
    return unknown;
}
void CppRenderer::initialize_environments(std::span<NodeId const> children) {
    auto& state{state_};
    collect_environment(state.environments.global(), children);
}
auto CppRenderer::type_symbol(CppTypeId type, EnvironmentId environment) -> SymbolBinding {
    type = ir_.types.unqualified_type(type);
    if (type.value == 0) {
        return {};
    }
    auto const shape{ir_.types.shapes_[type.value - 1]};
    if (shape.form == CppTypeForm::named) {
        auto const components{
            std::span{ir_.types.name_components_}.subspan(shape.parts.begin, shape.parts.count)};
        auto const resolved{resolve_name(components, shape.origin, environment)};
        if (resolved.prefix_size == components.size() && !resolved.symbol.ambiguous) {
            return resolved.symbol;
        }
    } else if (shape.form == CppTypeForm::raw) {
        auto const name{ir_.types.spelling_id(type)};
        auto const found{state_.environments.lookup(environment, std::span{&name, 1})};
        if (found.status == LookupStatus::found) {
            return found.symbol;
        }
    }
    return {};
}
auto CppRenderer::value_symbol(Syntax value) -> SymbolBinding {
    if (value.kind() == SyntaxKind::expression) {
        auto const expression{value.expression()};
        if (expression.kind() == ExprKind::symbol_reference) {
            return value_symbol(ir_.symbol_references_[expression.index()]);
        }
    } else if (value.kind() == SyntaxKind::name) {
        auto const name{ir_.named_exprs_.spelling[value.index()]};
        auto const found{state_.environments.lookup(state_.current, std::span{&name, 1})};
        if (found.status == LookupStatus::found) {
            return found.symbol;
        }
    } else if (value.kind() == SyntaxKind::scope_name) {
        auto const owner{type_symbol(ir_.scope_name_owners_[value.index()], state_.current)};
        auto const scope{owner.aliased_type.value != 0 ? alias_scope(owner)
                                                       : owner.introduced_scope};
        if (!scope.is_null()) {
            auto const found{
                state_.environments.lookup_local(scope, ir_.scope_name_members_[value.index()])};
            if (found.status == LookupStatus::found) {
                return found.symbol;
            }
        }
    }
    return {};
}
auto CppRenderer::declaration_destination(SymbolBinding symbol) -> TypeDestination {
    CppTypeId type{};
    if (symbol.declaration.kind() == CppDeclarationKind::parameter) {
        type = ir_.get_view(FunctionParameterId{symbol.declaration.index()}).type;
    } else if (auto const node{symbol.declaration.node()}) {
        auto const index{node->index()};
        switch (node->kind()) {
            case NodeKind::variable_declaration_stmt:
                type = ir_.variable_declaration_stmt_nodes_.type[index];
                break;
            case NodeKind::instance_member:
                type = ir_.instance_members_.type[index];
                break;
            case NodeKind::static_member:
                type = ir_.static_members_.type[index];
                break;
            case NodeKind::constexpr_member:
                type = ir_.constexpr_members_.type[index];
                break;
            default:
                break;
        }
    }
    return destination_type(type, symbol.declaring_scope);
}
auto CppRenderer::destination_type(CppTypeId type, EnvironmentId environment) -> TypeDestination {
    type = ir_.types.unqualified_type(type);
    if (type.value == 0 || ir_.types.integer_type(type).bits != 0) {
        return {type, environment};
    }
    auto const symbol{type_symbol(type, environment)};
    if (symbol.aliased_type.value != 0) {
        return destination_type(symbol.aliased_type, symbol.declaring_scope);
    }
    return {type, environment};
}
auto CppRenderer::value_destination(Expr value) -> TypeDestination {
    if (value.kind() == ExprKind::symbol_reference) {
        if (auto const type{declaration_destination(value_symbol(value))}; type.type.value != 0) {
            return type;
        }
    } else if (value.kind() == ExprKind::subscript) {
        auto const object_type{value_destination(ir_.subscript_exprs_.object[value.index()])};
        if (object_type.type.value != 0) {
            return destination_type(
                ir_.types.shapes_[object_type.type.value - 1].initializer_element_type,
                object_type.scope);
        }
    }
    return destination_type(ir_.expression_type(value), state_.current);
}

/* **************************************** */
// Expressions and declarations
/* **************************************** */
auto CppRenderer::expression(Expr expr, RenderContext const& ctx) -> std::string {
    std::string output;
    append_expr(output, expr, ctx);
    return output;
}
auto CppRenderer::expr_precedence(Expr expr) -> int {
    switch (expr.kind()) {
        case ExprKind::raw:
        case ExprKind::conditional:
            return 0;
        case ExprKind::binary:
            return expr_detail::binary_precedence(ir_.binary_exprs_.operation[expr.index()]);
        case ExprKind::sizeof_type:
        case ExprKind::unary:
            return 11;
        default:
            return 12;
    }
}
auto CppRenderer::brace_integer_type(CppTypeId type, EnvironmentId scope) -> IntegerType {
    auto const shape{ir_.types.shapes_[type.value - 1]};
    if (shape.unqualified_type.value != 0) {
        return {};
    }
    if (auto const integer{ir_.types.integer_type(type)}; integer.bits != 0) {
        return integer;
    }
    auto const symbol{type_symbol(type, scope)};
    return symbol.aliased_type.value != 0
             ? brace_integer_type(symbol.aliased_type, symbol.declaring_scope)
             : IntegerType{};
}
auto CppRenderer::integer_constant(Expr const value) const -> std::optional<IntegerConstant> {
    auto constant = [&](auto const number) -> IntegerConstant {
        bool const negative{std::cmp_less(number, 0)};
        auto const magnitude{negative ? std::uint64_t{0} - static_cast<std::uint64_t>(number)
                                      : static_cast<std::uint64_t>(number)};
        return {magnitude, negative, ir_.types.integer_type(ir_.expression_type(value))};
    };
    auto const index{value.index()};
    switch (value.kind()) {
        case ExprKind::i8_literal:
            return constant(ir_.numeric_literal_exprs_.i8[index]);
        case ExprKind::i16_literal:
            return constant(ir_.numeric_literal_exprs_.i16[index]);
        case ExprKind::i32_literal:
            return constant(ir_.numeric_literal_exprs_.i32[index]);
        case ExprKind::i64_literal:
            return constant(ir_.numeric_literal_exprs_.i64[index]);
        case ExprKind::u8_literal:
            return constant(ir_.numeric_literal_exprs_.u8[index]);
        case ExprKind::u16_literal:
            return constant(ir_.numeric_literal_exprs_.u16[index]);
        case ExprKind::u32_literal:
            return constant(ir_.numeric_literal_exprs_.u32[index]);
        case ExprKind::u64_literal:
            return constant(ir_.numeric_literal_exprs_.u64[index]);
        case ExprKind::parenthesized:
            return integer_constant(ir_.parenthesized_exprs_.operand[index]);
        case ExprKind::unary: {
            auto const operand{ir_.unary_exprs_.operand[index]};
            if (ir_.unary_exprs_.operation[index] != UnaryOperator::negate ||
                operand.kind() != SyntaxKind::expression) {
                return std::nullopt;
            }
            auto result{integer_constant(operand.expression())};
            if (!result) {
                return std::nullopt;
            }
            result->type = result->type.promoted();
            result->negative = !result->negative && result->magnitude != 0;
            if (!result->type.is_signed ||
                !result->type.contains(result->magnitude, result->negative)) {
                return std::nullopt;
            }
            return result;
        }
        case ExprKind::binary: {
            // Recognize the signed-minimum encoding without folding arbitrary arithmetic.
            if (ir_.binary_exprs_.operation[index] != BinaryOperator::subtract) {
                return std::nullopt;
            }
            auto result{integer_constant(ir_.binary_exprs_.left[index])};
            auto const right{integer_constant(ir_.binary_exprs_.right[index])};
            if (!result || !right || !result->negative || !result->type.is_signed ||
                result->magnitude != result->type.maximum() || right->negative ||
                right->magnitude != 1 || right->type != IntegerType{32, true}) {
                return std::nullopt;
            }
            ++result->magnitude;
            return result;
        }
        default:
            return std::nullopt;
    }
}
void CppRenderer::append_integer_constant(std::string& output, IntegerConstant const value) const {
    if (value.negative && value.magnitude == (std::uint64_t{1} << 63)) {
        output += "(-9223372036854775807 - 1)";
        return;
    }
    if (value.negative) {
        output += '-';
    }
    std::format_to(std::back_inserter(output), "{}", value.magnitude);
    if (!value.negative && value.magnitude > std::numeric_limits<std::int64_t>::max()) {
        output += "ULL";
    }
}
auto CppRenderer::append_initializer_literal(std::string& output,
                                             Expr const value,
                                             CppTypeId const destination) -> bool {
    auto const type{ir_.types.integer_type(destination)};
    auto const constant{integer_constant(value)};
    if (!constant || !type.contains(constant->magnitude, constant->negative)) {
        return false;
    }
    append_integer_constant(output, *constant);
    return true;
}
auto CppRenderer::append_conditional_initializer(std::string& output,
                                                 Expr value,
                                                 CppTypeId destination,
                                                 RenderContext const& ctx,
                                                 int minimum) -> bool {
    if (value.kind() == ExprKind::parenthesized) {
        return append_conditional_initializer(
            output, ir_.parenthesized_exprs_.operand[value.index()], destination, ctx, minimum);
    }
    if (value.kind() != ExprKind::conditional) {
        return false;
    }
    auto const index{value.index()};
    auto when_true{integer_constant(ir_.conditional_exprs_.when_true[index])};
    auto when_false{integer_constant(ir_.conditional_exprs_.when_false[index])};
    if (!when_true || !when_false) {
        return false;
    }

    auto const left{when_true->type.promoted()};
    auto const right{when_false->type.promoted()};
    auto const common{left.bits > right.bits ? left
                      : right.bits > left.bits
                          ? right
                          : IntegerType{left.bits, left.is_signed && right.is_signed}};
    auto convert = [&](IntegerConstant& constant) {
        if (!common.is_signed && constant.negative) {
            constant.magnitude = (std::uint64_t{0} - constant.magnitude) & common.maximum();
            constant.negative = false;
        }
    };
    convert(*when_true);
    convert(*when_false);
    auto const type{ir_.types.integer_type(destination)};
    if (!type.contains(when_true->magnitude, when_true->negative) ||
        !type.contains(when_false->magnitude, when_false->negative)) {
        return false;
    }

    // Apply the original common type before shortening either branch.
    if (minimum > 0) {
        output += '(';
    }
    append_expr(output, ir_.conditional_exprs_.condition[index], ctx, 1);
    output += " ? ";
    append_integer_constant(output, *when_true);
    output += " : ";
    append_integer_constant(output, *when_false);
    if (minimum > 0) {
        output += ')';
    }
    return true;
}
auto CppRenderer::call_destinations(Syntax callee, ir::Range arguments)
    -> std::vector<TypeDestination> {
    auto const symbol{value_symbol(callee)};
    if (symbol.ambiguous || symbol.overloaded ||
        symbol.declaration.kind() != CppDeclarationKind::function) {
        return {};
    }
    auto const function{ir_.get_view(FunctionId{symbol.declaration.index()})};
    if (function.template_declaration || arguments.count > function.parameters.size()) {
        return {};
    }
    // Integer arguments cannot introduce additional candidates through ADL.
    for (auto const argument : ir_.exprs(arguments)) {
        if (argument.kind() != SyntaxKind::expression ||
            (!integer_constant(argument.expression()) &&
             ir_.types.integer_type(value_destination(argument.expression()).type).bits == 0)) {
            return {};
        }
    }
    std::vector<TypeDestination> destinations;
    destinations.reserve(function.parameters.size());
    for (auto const parameter : function.parameters) {
        auto const type{destination_type(ir_.get_view(parameter).type, symbol.declaring_scope)};
        destinations.push_back(type);
    }
    return destinations;
}
auto CppRenderer::aggregate_members(TypeDestination type) -> std::vector<SymbolBinding> {
    auto symbol{type_symbol(type.type, type.scope)};
    while (symbol.aliased_type.value != 0) {
        symbol = type_symbol(symbol.aliased_type, symbol.declaring_scope);
    }
    auto const node{symbol.declaration.node()};
    if (!node || node->kind() != NodeKind::struct_) {
        return {};
    }
    auto const index{node->index()};
    if (ir_.struct_nodes_.bases[index].count != 0 ||
        ir_.struct_nodes_.template_declaration[index]) {
        return {};
    }
    bool public_members{ir_.struct_nodes_.record_kind[index] == RecordKind::struct_};
    std::vector<SymbolBinding> members;
    for (auto const child : ir_.nodes(ir_.struct_nodes_.children[index])) {
        switch (child.kind()) {
            case NodeKind::instance_member:
                if (!public_members) {
                    return {};
                }
                members.push_back({.declaration = CppDeclarationId{child},
                                   .declaring_scope = symbol.introduced_scope});
                break;
            case NodeKind::access_specifier:
                public_members =
                    ir_.access_specifier_nodes_.access[child.index()] == Access::public_;
                break;
            case NodeKind::using_declaration:
            case NodeKind::static_member:
            case NodeKind::constexpr_member:
            case NodeKind::line_comment:
            case NodeKind::new_lines:
            case NodeKind::static_assert_:
                break;
            default:
                return {};
        }
    }
    return members;
}
void CppRenderer::append_initializer(std::string& output,
                                     Syntax const value,
                                     CppTypeId const destination,
                                     RenderContext const& ctx,
                                     int const minimum) {
    append_initializer(output, value, TypeDestination{destination, state_.current}, ctx, minimum);
}
void CppRenderer::append_initializer(std::string& output,
                                     Syntax const value,
                                     TypeDestination destination,
                                     RenderContext const& ctx,
                                     int const minimum) {
    destination = destination_type(destination.type, destination.scope);
    if (value.kind() == SyntaxKind::expression) {
        if (append_initializer_literal(output, value.expression(), destination.type) ||
            append_conditional_initializer(
                output, value.expression(), destination.type, ctx, minimum)) {
            return;
        }
    }
    if (value.kind() == SyntaxKind::initializer_list) {
        output += '{';
        append_initializer_elements(
            output, ir_.initializer_exprs_.elements[value.index()], destination, ctx);
        output += '}';
        return;
    }
    append_expr(output, value, ctx, minimum);
}
void CppRenderer::append_initializer_elements(std::string& output,
                                              ir::Range const elements,
                                              TypeDestination destination,
                                              RenderContext const& ctx) {
    destination = destination_type(destination.type, destination.scope);
    auto element_type{destination};
    if (element_type.type.value != 0) {
        if (auto const type{
                ir_.types.shapes_[element_type.type.value - 1].initializer_element_type};
            type.value != 0) {
            element_type = destination_type(type, destination.scope);
        }
    }
    auto const members{aggregate_members(destination)};
    // Brace elision can consume several values for one non-scalar member.
    auto const positional_members_known{std::ranges::all_of(members, [&](SymbolBinding member) {
        return ir_.types.integer_type(declaration_destination(member).type).bits != 0;
    })};
    std::size_t member_index{};
    bool first{true};
    for (auto const element : ir_.exprs(elements)) {
        if (!first) {
            output += ", ";
        }
        auto type{element_type};
        auto value{element};
        if (element.kind() == SyntaxKind::designated_initializer && !members.empty()) {
            auto const name{ir_.designated_initializers_.member[element.index()]};
            auto const member{std::ranges::find_if(members, [&](SymbolBinding candidate) {
                return ir_.instance_members_.name[candidate.declaration.index()] == name;
            })};
            type = member != members.end() ? declaration_destination(*member) : TypeDestination{};
            value = ir_.designated_initializers_.value[element.index()];
            output += '.';
            output += ir_.strings.view(name);
            output += " = ";
        } else if (positional_members_known && member_index < members.size()) {
            type = declaration_destination(members[member_index]);
        }
        if (!members.empty() && !positional_members_known &&
            element.kind() != SyntaxKind::designated_initializer) {
            type = {};
        }
        append_initializer(output, value, type, ctx, 1);
        ++member_index;
        first = false;
    }
}
void CppRenderer::append_expr(
    std::string& output, Expr expr, RenderContext const& ctx, int minimum, bool equal) {
    auto const index{expr.index()};
    auto const precedence{expr_precedence(expr)};
    auto const parenthesize{precedence < minimum || (equal && precedence == minimum)};
    if (parenthesize) {
        output += '(';
    }
    auto append_arguments = [&](ir::Range range) {
        bool first{true};
        for (auto const argument : ir_.exprs(range)) {
            if (!first) {
                output += ", ";
            }
            append_expr(output, argument, ctx, 1);
            first = false;
        }
    };
    auto append_small_literal = [&](auto value) {
        append_type(output, ir_.expression_type(expr), ctx);
        output += '{';
        expr_detail::append_numeric_literal(output, +value);
        output += '}';
    };
    switch (expr.kind()) {
        case ExprKind::parenthesized:
            output += '(';
            append_expr(output, ir_.parenthesized_exprs_.operand[index], ctx);
            output += ')';
            break;
        case ExprKind::literal:
            output += ir_.strings.view(ir_.literal_exprs_.spelling[index]);
            break;
        case ExprKind::boolean_literal:
            output += index != 0 ? "true" : "false";
            break;
        case ExprKind::i8_literal:
            append_small_literal(ir_.numeric_literal_exprs_.i8[index]);
            break;
        case ExprKind::i16_literal:
            append_small_literal(ir_.numeric_literal_exprs_.i16[index]);
            break;
        case ExprKind::i32_literal:
            expr_detail::append_numeric_literal(output, ir_.numeric_literal_exprs_.i32[index]);
            break;
        case ExprKind::i64_literal:
            expr_detail::append_numeric_literal(output, ir_.numeric_literal_exprs_.i64[index]);
            break;
        case ExprKind::u8_literal:
            append_small_literal(ir_.numeric_literal_exprs_.u8[index]);
            break;
        case ExprKind::u16_literal:
            append_small_literal(ir_.numeric_literal_exprs_.u16[index]);
            break;
        case ExprKind::u32_literal:
            expr_detail::append_numeric_literal(output, ir_.numeric_literal_exprs_.u32[index]);
            break;
        case ExprKind::u64_literal:
            expr_detail::append_numeric_literal(output, ir_.numeric_literal_exprs_.u64[index]);
            break;
        case ExprKind::f32_literal:
            expr_detail::append_numeric_literal(output, ir_.numeric_literal_exprs_.f32[index]);
            break;
        case ExprKind::f64_literal:
            expr_detail::append_numeric_literal(output, ir_.numeric_literal_exprs_.f64[index]);
            break;
        case ExprKind::string_literal:
            output += '"';
            append_escaped_cpp_string(output, ir_.strings.view(ir_.string_literal_exprs_[index]));
            output += '"';
            break;
        case ExprKind::raw:
            output += ir_.strings.view(ir_.raw_exprs_.text[index]);
            break;
        case ExprKind::call: {
            append_expr(output, ir_.call_exprs_.callee[index], ctx, 12);
            output += '(';
            auto const arguments{ir_.call_exprs_.arguments[index]};
            auto const destinations{call_destinations(ir_.call_exprs_.callee[index], arguments)};
            std::size_t argument_index{};
            for (auto const argument : ir_.exprs(arguments)) {
                if (argument_index != 0) {
                    output += ", ";
                }
                auto const type{argument_index < destinations.size() ? destinations[argument_index]
                                                                     : TypeDestination{}};
                append_initializer(output, argument, type, ctx, 1);
                ++argument_index;
            }
            output += ')';
            break;
        }
        case ExprKind::member_access:
            append_expr(output, ir_.member_exprs_.object[index], ctx, 12);
            output += ir_.member_exprs_.through_pointer[index] ? "->" : ".";
            output += ir_.strings.view(ir_.member_exprs_.member[index]);
            break;
        case ExprKind::subscript:
            append_expr(output, ir_.subscript_exprs_.object[index], ctx, 12);
            output += '[';
            append_expr(output, ir_.subscript_exprs_.index[index], ctx, 1);
            output += ']';
            break;
        case ExprKind::binary: {
            auto const operation{ir_.binary_exprs_.operation[index]};
            auto append_operand = [&](Expr operand, bool right) {
                auto const conjunction{
                    operation == BinaryOperator::logical_or && operand.kind() == ExprKind::binary &&
                    ir_.binary_exprs_.operation[operand.index()] == BinaryOperator::logical_and};
                append_expr(output,
                            operand,
                            ctx,
                            conjunction ? expr_precedence(operand) : precedence,
                            right || conjunction);
            };
            append_operand(ir_.binary_exprs_.left[index], false);
            output += ' ';
            output += to_serialized_string(operation);
            output += ' ';
            if (ir_.binary_exprs_.line_break_after[index]) {
                output.pop_back();
                output += "\n    ";
            }
            append_operand(ir_.binary_exprs_.right[index], true);
            break;
        }
        case ExprKind::unary: {
            auto const operation{ir_.unary_exprs_.operation[index]};
            auto const operand{ir_.unary_exprs_.operand[index]};
            output += to_serialized_string(operation);
            append_expr(output, operand, ctx, 11, true);
            break;
        }
        case ExprKind::static_cast_:
        case ExprKind::reinterpret_cast_: {
            auto const target{ir_.cast_exprs_.type[index]};
            auto const operand{ir_.cast_exprs_.operand[index]};
            if (expr.kind() == ExprKind::static_cast_) {
                auto const constant{integer_constant(operand)};
                auto const integer{brace_integer_type(target, state_.current)};
                auto const spelling{ir_.types.spelling(target)};
                // A multiword or qualified type-id cannot be written as T{value} directly.
                if (constant && integer.contains(constant->magnitude, constant->negative) &&
                    spelling.find_first_of(" \t\r\n*&") == std::string_view::npos) {
                    append_type(output, target, ctx);
                    output += '{';
                    append_integer_constant(output, *constant);
                    output += '}';
                    break;
                }
            }
            output += expr.kind() == ExprKind::static_cast_ ? "static_cast<" : "reinterpret_cast<";
            append_type(output, target, ctx);
            output += ">(";
            if (expr.kind() == ExprKind::static_cast_) {
                append_initializer(output, operand, target, ctx, 1);
            } else {
                append_expr(output, operand, ctx, 1);
            }
            output += ')';
            break;
        }
        case ExprKind::conditional:
            append_expr(output, ir_.conditional_exprs_.condition[index], ctx, 1);
            output += " ? ";
            append_expr(output, ir_.conditional_exprs_.when_true[index], ctx);
            output += " : ";
            append_expr(output, ir_.conditional_exprs_.when_false[index], ctx);
            break;
        case ExprKind::sizeof_type:
            output += "sizeof(";
            append_type(output, ir_.sizeof_exprs_.type[index], ctx);
            output += ')';
            break;
        case ExprKind::initializer_list:
            if (auto const type{ir_.initializer_exprs_.type[index]}) {
                append_type(output, *type, ctx);
            }
            output += '{';
            append_initializer_elements(
                output,
                ir_.initializer_exprs_.elements[index],
                {ir_.initializer_exprs_.type[index].value_or(CppTypeId{}), state_.current},
                ctx);
            output += '}';
            break;
        case ExprKind::lambda: {
            output += '[';
            append_arguments(ir_.lambda_exprs_.captures[index]);
            output += ']';
            EnvironmentGuard const guard{
                state_.current,
                state_.environments.append(state_.current, EnvironmentKind::function)};
            if (auto const declaration{ir_.lambda_exprs_.template_declaration[index]}) {
                append_template(output, declaration, ctx, false);
            }
            append_parameters(
                output,
                ir::range_values(ir_.parameter_references_, ir_.lambda_exprs_.parameters[index]),
                ctx,
                true);
            auto const body{ir_.nodes(ir_.lambda_exprs_.body[index])};
            state_.current = block_environment(body);
            output += " {";
            if (!body.empty()) {
                output += '\n';
                auto body_context{ctx.indent()};
                body_context.return_destination = {};
                append_nodes(output, body, body_context, 1);
                output += '\n';
                append_indent(output, ctx);
            }
            output += '}';
            break;
        }
        case ExprKind::symbol_reference:
            append_expr(output, ir_.symbol_references_[index], ctx);
            break;
        case ExprKind::Count:
        case ExprKind::invalid:
            std::unreachable();
    }
    if (parenthesize) {
        output += ')';
    }
}
void CppRenderer::append_template(std::string& output,
                                  TemplateId declaration,
                                  RenderContext const& ctx,
                                  bool keyword) {
    assert(declaration);
    auto const row{declaration.index()};
    output += keyword ? "template <" : "<";
    if (declaration.kind() == TemplateKind::authored_raw) {
        output += ir_.strings.view(ir_.templates_.authored_text[row]);
        state_.environments.mark_unknown(state_.current);
    } else if (declaration.kind() == TemplateKind::parameters) {
        auto const range{ir_.templates_.parameters[row]};
        auto const end{range.begin + range.count};
        for (auto position{range.begin}; position < end; ++position) {
            if (position != range.begin) {
                output += ", ";
            }
            auto const parameter{ir_.template_parameter_references_[position]};
            auto const index{parameter.index()};
            switch (parameter.kind()) {
                case TemplateParameterKind::type: {
                    output += "typename";
                    if (ir_.type_template_parameters_.pack[index]) {
                        output += "...";
                    }
                    auto const name{ir_.type_template_parameters_.name[index]};
                    if (name.value != 0) {
                        output += ' ';
                        output += ir_.strings.view(name);
                    }
                    auto const default_type{ir_.type_template_parameters_.default_type[index]};
                    if (default_type.value != 0) {
                        output += " = ";
                        append_type(output, default_type, ctx);
                    }
                    break;
                }
                case TemplateParameterKind::value: {
                    append_type(output, ir_.value_template_parameters_.type[index], ctx);
                    if (ir_.value_template_parameters_.pack[index]) {
                        output += "...";
                    }
                    auto const name{ir_.value_template_parameters_.name[index]};
                    if (name.value != 0) {
                        output += ' ';
                        output += ir_.strings.view(name);
                    }
                    if (auto const value{ir_.value_template_parameters_.default_value[index]}) {
                        output += " = ";
                        append_initializer(
                            output, value, ir_.value_template_parameters_.type[index], ctx);
                    }
                    break;
                }
            }
            auto const name{parameter.kind() == TemplateParameterKind::type
                                ? ir_.type_template_parameters_.name[index]
                                : ir_.value_template_parameters_.name[index]};
            if (name.value != 0) {
                state_.environments.declare(state_.current, name, CppDeclarationId{parameter});
            }
        }
    }
    output += '>';
}
void CppRenderer::append_parameters(std::string& output,
                                    std::span<FunctionParameterId const> parameters,
                                    RenderContext const& ctx,
                                    bool defaults) {
    output += '(';
    auto const count{parameters.size()};
    for (std::size_t p{}; p < count; ++p) {
        if (p != 0) {
            output += ", ";
        }
        auto const parameter{parameters[p]};
        auto const view{ir_.get_view(parameter)};
        append_type(output, view.type, ctx);
        auto const name{view.name};
        if (name.value != 0) {
            output += ' ';
            output += ir_.strings.view(name);
            state_.environments.declare(state_.current, name, CppDeclarationId{parameter});
        }
        if (auto const value{view.default_value}; value && defaults) {
            output += " = ";
            append_initializer(output, value, view.type, ctx);
        }
    }
    output += ')';
}
auto CppRenderer::signature(std::uint32_t index,
                            RenderContext const& ctx,
                            EnvironmentId& environment) -> std::string {
    auto const& nodes{ir_.function_nodes_};
    auto const function{ir_.get_view(nodes.function[index])};
    auto const declaration{nodes.declaration[index] != 0};
    auto const header{nodes.is_header[index] != 0};
    auto const owner{nodes.owner[index]};
    auto const format{function.formatting};
    auto const implementation{function.implementation};
    if (owner && implementation.kind == FunctionImplementationKind::deleted) {
        throw std::invalid_argument{"Function '" + expression(function.name, ctx) +
                                    "' deleted function cannot be an out-of-line definition"};
    }
    auto const return_syntax{nodes.return_syntax[index].value_or(function.return_syntax)};
    auto const receiver{function.receiver};
    std::string output;
    if (function.flags.is_nodiscard()) {
        output += "[[nodiscard]] ";
    }
    if (function.flags.is_noreturn()) {
        output += "[[noreturn]] ";
    }
    auto& state{state_};
    EnvironmentGuard const guard{state.current, state.current};
    auto const enclosing{state.current};
    auto const template_environment{
        state.environments.append(enclosing, EnvironmentKind::template_)};
    state.current = template_environment;
    std::string template_text;
    if (auto const declaration{function.template_declaration}) {
        append_template(template_text, declaration, ctx);
    }
    if (function.flags.is_inline() && header && (owner || !in_record_scope(enclosing))) {
        output += "inline ";
    }
    if (receiver == FunctionReceiver::static_ && !owner) {
        output += "static ";
    }
    if (function.flags.is_constexpr()) {
        output += "constexpr ";
    }
    auto const export_specifier{(declaration || header) ? function.export_specifier : StringId{}};
    if (export_specifier.value != 0 && !format.export_after_return_type) {
        output += ir_.strings.view(export_specifier);
        output += ' ';
    }
    if (function.kind == FunctionKind::ordinary) {
        if (return_syntax == ReturnSyntax::trailing) {
            output += "auto";
        } else {
            append_type(output, function.return_type, ctx);
        }
        output += ' ';
    } else if (function.kind == FunctionKind::explicit_constructor && !owner) {
        output += "explicit ";
    }
    if (export_specifier.value != 0 && format.export_after_return_type) {
        output += ir_.strings.view(export_specifier);
        output += ' ';
    }
    if (owner) {
        append_type(output, *owner, ctx);
        output += "::";
    }
    append_expr(output, function.name, ctx);
    state.current = enclosing;
    if (owner) {
        state.current = record_environment(ir_.types.spelling(*owner));
    }
    auto const owner_environment{state.current};
    auto const function_environment{
        state.environments.append(state.current, EnvironmentKind::function)};
    state.environments.import_symbols(function_environment, template_environment);
    state.current = function_environment;
    environment = function_environment;
    append_parameters(output, function.parameters, ctx, declaration || header);
    switch (receiver) {
        case FunctionReceiver::unqualified:
        case FunctionReceiver::static_:
            break;
        case FunctionReceiver::const_:
            output += " const";
            break;
        case FunctionReceiver::lvalue:
            output += " &";
            break;
        case FunctionReceiver::const_lvalue:
            output += " const &";
            break;
        case FunctionReceiver::rvalue:
            output += " &&";
            break;
        case FunctionReceiver::const_rvalue:
            output += " const &&";
            break;
    }
    auto const exception{function.exception_specification};
    switch (exception.kind) {
        case FunctionExceptionKind::potentially_throwing:
            break;
        case FunctionExceptionKind::noexcept_:
            output += " noexcept";
            break;
        case FunctionExceptionKind::conditional:
            output += " noexcept(";
            append_expr(output, exception.condition, ctx);
            output += ')';
            break;
    }
    if (function.kind == FunctionKind::ordinary && return_syntax == ReturnSyntax::trailing) {
        output += format.trailing_return_placement == FunctionTrailingReturnPlacement::next_line
                    ? "\n    -> "
                    : " -> ";
        append_type(output, function.return_type, ctx);
    }
    if (auto const clause{function.requires_clause}) {
        auto const value{expression(clause, ctx)};
        switch (format.requires_placement) {
            case FunctionRequiresPlacement::trailing_next_line:
                output += "\n    requires ";
                output += value;
                break;
            case FunctionRequiresPlacement::trailing_same_line:
                output += " requires ";
                output += value;
                break;
            case FunctionRequiresPlacement::before_signature:
                output.insert(0, "    requires " + std::string{value} + "\n");
                break;
        }
    }
    if (!declaration && implementation.kind == FunctionImplementationKind::body) {
        auto const initializers{implementation.initializers};
        auto const count{initializers.size()};
        for (std::size_t m{}; m < count; ++m) {
            auto const row{initializers[m].value};
            output += m == 0 ? "\n    : " : "\n    , ";
            output += ir_.strings.view(ir_.initializers_.member[row]);
            output += '{';
            auto const member{
                state.environments.lookup_local(owner_environment, ir_.initializers_.member[row])};
            auto const type{member.status == LookupStatus::found
                                ? declaration_destination(member.symbol)
                                : TypeDestination{}};
            append_initializer(output, ir_.initializers_.value[row], type, ctx);
            output += '}';
        }
    }
    if (!template_text.empty()) {
        template_text +=
            format.template_placement == FunctionTemplatePlacement::same_line ? " " : "\n";
        output.insert(0, template_text);
    }
    if (ioj::is_any(implementation.kind,
                    {FunctionImplementationKind::defaulted, FunctionImplementationKind::deleted})) {
        output += format.disposition_placement == FunctionDispositionPlacement::next_line ? "\n= "
                                                                                          : " = ";
        output +=
            implementation.kind == FunctionImplementationKind::defaulted ? "default" : "delete";
    }
    return output;
}
void CppRenderer::append_node(std::string& output, NodeId node, RenderContext const& ctx) {
    auto& state{state_};
    EnvironmentGuard const scope_guard{state.current, state.current};
    auto const enclosing{state.current};
    auto const i{node.index()};
    auto indent = [&] { append_indent(output, ctx); };
    auto write = [&](std::string_view text) { append_indented(output, text, ctx); };
    auto write_prefix = [&](std::string_view text) {
        indent();
        output += text;
    };
    auto close = [&](std::string_view text) {
        output += '\n';
        indent();
        output += text;
    };
    auto type = [&](CppTypeId id) { append_type(output, id, ctx); };
    auto text = [&](StringId id) { output += ir_.strings.view(id); };
    auto expr = [&](Syntax id) { append_expr(output, id, ctx); };
    auto scoped_nodes = [&](ir::Range children) {
        EnvironmentGuard const guard{state.current, block_environment(ir_.nodes(children))};
        append_nodes(output, ir_.nodes(children), ctx.indent(), 1);
    };
    auto template_prefix = [&](TemplateId declaration) {
        if (declaration) {
            state.current = state.environments.append(state.current, EnvironmentKind::template_);
            indent();
            append_template(output, declaration, ctx);
            output += '\n';
        }
    };
    auto include = [&](std::string_view path, bool system) {
        write_prefix("#include ");
        output += system ? '<' : '"';
        output += path;
        output += system ? '>' : '"';
    };
    auto initialize = [&](Syntax value, CppTypeId destination) {
        auto const direct_list{value.kind() == SyntaxKind::initializer_list};
        if (!direct_list) {
            output += '{';
        }
        append_initializer(output, value, destination, ctx);
        if (!direct_list) {
            output += '}';
        }
    };
    auto member = [&](auto const& members, std::string_view prefix) {
        indent();
        output += prefix;
        type(members.type[i]);
        output += ' ';
        output += ir_.strings.view(members.name[i]);
        if (auto const initializer{members.initializer[i]}) {
            initialize(initializer, members.type[i]);
        }
        output += ';';
    };
    switch (node.kind()) {
        case NodeKind::raw: {
            std::string rendered{ir_.strings.view(ir_.raw_nodes_.text[i])};
            auto part_context{ctx};
            part_context.indent_level = 0;
            for (auto const part : ir_.exprs(ir_.raw_nodes_.parts[i])) {
                append_expr(rendered, part, part_context);
            }
            write(rendered);
            break;
        }
        case NodeKind::raw_lines: {
            auto const range{ir_.raw_lines_nodes_.lines[i]};
            auto const end{range.begin + range.count};
            for (auto line{range.begin}; line < end; ++line) {
                if (line != range.begin) {
                    output += '\n';
                }
                write(ir_.strings.view(ir_.raw_lines_[line]));
            }
            break;
        }
        case NodeKind::line_comment:
            write("// " + std::string{ir_.strings.view(ir_.line_comment_nodes_.text[i])});
            break;
        case NodeKind::line_directive:
            std::format_to(
                std::back_inserter(output), "#line {} \"", ir_.line_directive_nodes_.line[i]);
            append_escaped_cpp_string(output, ir_.strings.view(ir_.line_directive_nodes_.path[i]));
            output += '"';
            break;
        case NodeKind::block_comment:
            write("/* " + std::string{ir_.strings.view(ir_.block_comment_nodes_.text[i])} + " */");
            break;
        case NodeKind::block:
            write("{");
            output += '\n';
            state.current = block_environment(ir_.nodes(ir_.block_nodes_.children[i]));
            append_nodes(output, ir_.nodes(ir_.block_nodes_.children[i]), ctx.indent(), 1);
            close("}");
            break;
        case NodeKind::if_stmt:
            write_prefix(ir_.if_stmt_nodes_.is_constexpr[i] ? "if constexpr (" : "if (");
            expr(ir_.if_stmt_nodes_.condition[i]);
            output += ") {\n";
            scoped_nodes(ir_.if_stmt_nodes_.then_children[i]);
            close("}");
            if (auto const otherwise{ir_.if_stmt_nodes_.else_children[i]}) {
                output += " else {\n";
                scoped_nodes(*otherwise);
                close("}");
            }
            break;
        case NodeKind::for_stmt: {
            auto const initializer{ir_.for_stmt_nodes_.initializer[i]};
            state.current = block_environment(std::span{&initializer, 1});
            write_prefix("for (");
            auto initializer_context{ctx};
            initializer_context.indent_level = 0;
            append_node(output, initializer, initializer_context);
            output += ' ';
            expr(ir_.for_stmt_nodes_.condition[i]);
            output += "; ";
            expr(ir_.for_stmt_nodes_.increment[i]);
            output += ") {\n";
            scoped_nodes(ir_.for_stmt_nodes_.children[i]);
            close("}");
            break;
        }
        case NodeKind::switch_stmt: {
            write_prefix("switch (");
            expr(ir_.switch_stmt_nodes_.condition[i]);
            output += ") {";
            auto const range{ir_.switch_stmt_nodes_.cases[i]};
            for (auto const case_id : ir::range_values(ir_.case_references_, range)) {
                output += '\n';
                indent();
                if (auto const label{ir_.cases_.label[case_id.value]}) {
                    output += "case ";
                    expr(*label);
                } else {
                    output += "default";
                }
                auto const row{case_id.value};
                auto const braced{ir_.cases_.braced[row]};
                output += braced ? ": {\n" : ":\n";
                scoped_nodes(ir_.cases_.body[row]);
                if (braced) {
                    close("}");
                }
            }
            close("}");
            break;
        }
        case NodeKind::break_stmt:
            write("break;");
            break;
        case NodeKind::expr_stmt:
            indent();
            expr(ir_.expr_stmt_nodes_.expr[i]);
            output += ';';
            break;
        case NodeKind::declaration_macro:
            indent();
            expr(ir_.declaration_macro_nodes_.invocation[i]);
            if (auto const body{ir_.declaration_macro_nodes_.body[i]}) {
                auto const children{ir_.nodes(ir_.block_nodes_.children[body->index()])};
                state.current = block_environment(children);
                output += " {\n";
                append_nodes(output, children, ctx.indent(), 1);
                output += '\n';
                indent();
                output += '}';
            } else if (ir_.declaration_macro_nodes_.statement[i]) {
                output += ';';
            }
            break;
        case NodeKind::return_stmt:
            write_prefix("return");
            if (auto const value{ir_.return_stmt_nodes_.expr[i]}) {
                output += ' ';
                append_initializer(output, *value, ctx.return_destination, ctx);
            }
            output += ';';
            break;
        case NodeKind::assignment_stmt:
            indent();
            expr(ir_.assignment_stmt_nodes_.target[i]);
            output += " = ";
            append_initializer(output,
                               ir_.assignment_stmt_nodes_.value[i],
                               value_destination(ir_.assignment_stmt_nodes_.target[i]),
                               ctx);
            output += ';';
            break;
        case NodeKind::variable_declaration_stmt: {
            auto const& variables{ir_.variable_declaration_stmt_nodes_};
            template_prefix(variables.template_declaration[i]);
            indent();
            if (variables.is_inline[i]) {
                output += "inline ";
            }
            if (variables.is_constexpr[i]) {
                output += "constexpr ";
            }
            type(variables.type[i]);
            output += ' ';
            expr(variables.name[i]);
            declare_name(enclosing, variables.name[i], CppDeclarationId{node});
            if (auto const initializer{variables.initializer[i]}) {
                initialize(*initializer, variables.type[i]);
            } else {
                output += "{}";
            }
            output += ';';
            break;
        }
        case NodeKind::static_assert_:
            write_prefix("static_assert(");
            expr(ir_.static_assert_nodes_.condition[i]);
            if (auto const message{ir_.strings.view(ir_.static_assert_nodes_.message[i])};
                !message.empty()) {
                output += ", \"";
                output += escape_cpp_string(message);
                output += '"';
            }
            output += ");";
            break;
        case NodeKind::new_lines:
            output.append(static_cast<std::size_t>(ir_.new_lines_nodes_.count[i]), '\n');
            break;
        case NodeKind::access_specifier: {
            auto const begin{output.size()};
            indent();
            if (ir_.access_specifier_nodes_.indentation[i] == AccessIndentation::outdented &&
                output.size() - begin >= 2) {
                output.resize(output.size() - 2);
            }
            output += to_serialized_string(ir_.access_specifier_nodes_.access[i]);
            output += ':';
            break;
        }
        case NodeKind::include: {
            auto const path{ir_.strings.view(ir_.include_nodes_.path[i])};
            include(path, include_is_system(path, ir_.include_nodes_.style[i]));
            break;
        }
        case NodeKind::include_dependencies: {
            bool first_group{true};
            for (auto const& group : ctx.include_groups) {
                if (!first_group) {
                    output += "\n\n";
                }
                bool first{true};
                for (auto const& path : group) {
                    if (!first) {
                        output += '\n';
                    }
                    include(path, include_is_system(path));
                    first = false;
                }
                first_group = false;
            }
            break;
        }
        case NodeKind::forward_declaration:
            template_prefix(ir_.forward_declaration_nodes_.template_declaration[i]);
            indent();
            output += to_serialized_string(ir_.forward_declaration_nodes_.kind[i]);
            output += ' ';
            if (auto const specifier{ir_.forward_declaration_nodes_.export_specifier[i]};
                specifier.value != 0) {
                text(specifier);
                output += ' ';
            }
            text(ir_.forward_declaration_nodes_.name[i]);
            output += ';';
            break;
        case NodeKind::friend_declaration:
            template_prefix(ir_.friend_declaration_nodes_.template_declaration[i]);
            write_prefix("friend ");
            if (auto const friend_type{ir_.friend_declaration_nodes_.type[i]};
                friend_type.value != 0) {
                type(friend_type);
            } else {
                output += to_serialized_string(ir_.friend_declaration_nodes_.kind[i]);
                output += ' ';
                text(ir_.friend_declaration_nodes_.name[i]);
            }
            output += ';';
            break;
        case NodeKind::using_declaration:
            if (ir_.using_declaration_nodes_.name[i].value == 0) {
                auto const imported{ir_.using_declaration_nodes_.type[i]};
                auto const shape{ir_.types.shapes_[imported.value - 1]};
                auto const components{std::span{ir_.types.name_components_}.subspan(
                    shape.parts.begin, shape.parts.count)};
                auto const target{resolve_name(components, shape.origin, state_.current)};
                if (target.prefix_size == components.size() &&
                    target.symbol.declaring_scope == state_.current &&
                    !in_record_scope(state_.current)) {
                    break;
                }
                write_prefix("using ");
                append_name(output, components.first(components.size() - 1), shape.origin);
                output += "::";
                text(components.back());
                output += ';';
                break;
            }
            template_prefix(ir_.using_declaration_nodes_.template_declaration[i]);
            write_prefix("using ");
            text(ir_.using_declaration_nodes_.name[i]);
            output += " = ";
            type(ir_.using_declaration_nodes_.type[i]);
            output += ';';
            break;
        case NodeKind::instance_member:
            member(ir_.instance_members_, "");
            break;
        case NodeKind::static_member: {
            template_prefix(ir_.static_members_.template_declaration[i]);
            member(ir_.static_members_,
                   ir_.static_members_.is_inline[i] ? "inline static " : "static ");
            break;
        }
        case NodeKind::constexpr_member: {
            template_prefix(ir_.constexpr_members_.template_declaration[i]);
            member(ir_.constexpr_members_, "static constexpr ");
            break;
        }
        case NodeKind::function: {
            auto const function{ir_.get_view(ir_.function_nodes_.function[i])};
            EnvironmentId function_environment;
            write(signature(i, ctx, function_environment));
            auto const implementation{function.implementation};
            if (ir_.function_nodes_.declaration[i] ||
                implementation.kind != FunctionImplementationKind::body) {
                output += ';';
                break;
            }
            auto const body{implementation.body};
            state.current = function_environment;
            state.current = block_environment(body);
            auto const format{function.formatting};
            auto body_context{ctx};
            body_context.return_destination =
                destination_type(function.return_type, function_environment);
            if (format.body_layout == FunctionBodyLayout::compact) {
                body_context.indent_level = 0;
                output += " { ";
                append_nodes(output, body, body_context, 1);
                output += " }";
            } else {
                if (format.opening_brace_placement ==
                    FunctionOpeningBracePlacement::separate_line) {
                    close("{");
                } else {
                    output += " {";
                }
                output += '\n';
                append_nodes(output, body, body_context.indent(), 1);
                close("}");
            }
            break;
        }
        case NodeKind::enum_: {
            write_prefix("enum");
            if (ir_.enum_nodes_.scoped[i]) {
                output += " class";
            }
            if (auto const specifier{ir_.enum_nodes_.export_specifier[i]}; specifier.value != 0) {
                output += ' ';
                output += ir_.strings.view(specifier);
            }
            output += ' ';
            text(ir_.enum_nodes_.name[i]);
            if (auto const underlying{ir_.enum_nodes_.underlying_type[i]};
                underlying.value != 0 && !ir_.types.spelling(underlying).empty()) {
                output += " : ";
                type(underlying);
            }
            output += " {\n";
            auto const range{ir_.enum_nodes_.values[i]};
            auto const end{range.begin + range.count};
            for (auto e{range.begin}; e < end; ++e) {
                auto const row{ir_.enumerator_references_[e].value};
                append_indented(output, ir_.strings.view(ir_.enumerators_.name[row]), ctx.indent());
                if (auto const initializer{ir_.enumerators_.initializer[row]}) {
                    output += " = ";
                    text(*initializer);
                }
                if (auto const annotation{ir_.enumerators_.annotation[row]}) {
                    output += ' ';
                    text(*annotation);
                }
                output += ',';
                if (e + 1 < end) {
                    output += '\n';
                }
            }
            close("};");
            break;
        }
        case NodeKind::struct_: {
            if (auto const parameters{ir_.struct_nodes_.template_declaration[i]}) {
                template_prefix(parameters);
                if (auto const clause{ir_.struct_nodes_.requires_clause[i]}) {
                    write_prefix("    requires ");
                    expr(clause);
                    output += '\n';
                }
            }
            indent();
            output += to_serialized_string(ir_.struct_nodes_.record_kind[i]);
            if (auto const specifier{ir_.struct_nodes_.export_specifier[i]}; specifier.value != 0) {
                output += ' ';
                output += ir_.strings.view(specifier);
            }
            output += ' ';
            expr(ir_.struct_nodes_.name[i]);
            bool first{true};
            for (auto const base : ir_.type_list(ir_.struct_nodes_.bases[i])) {
                output += first ? " : " : ", ";
                type(base);
                first = false;
            }
            auto member_context{ctx.indent()};
            state.current = state.node_environments.at(RenderState::node_key(node, enclosing));
            output += " {\n";
            append_nodes(output, ir_.nodes(ir_.struct_nodes_.children[i]), member_context, 2);
            close("};");
            break;
        }
        case NodeKind::namespace_: {
            std::string const name{ir_.strings.view(ir_.namespace_nodes_.name[i])};
            write_prefix("namespace ");
            output += name;
            output += name.empty() ? "{\n" : " {\n";
            state.current = state.node_environments.at(RenderState::node_key(node, enclosing));
            append_nodes(output, ir_.nodes(ir_.namespace_nodes_.children[i]), ctx, 2);
            close("} // namespace");
            if (!name.empty()) {
                output += ' ';
                output += name;
            }
            break;
        }
        case NodeKind::invalid:
            std::unreachable();
    }
}
void CppRenderer::append_nodes(std::string& output,
                               std::span<NodeId const> values,
                               RenderContext const& ctx,
                               int newlines) {
    auto const start{output.size()};
    for (auto const node : values) {
        if (node.kind() == NodeKind::new_lines) {
            while (output.size() > start && output.back() == '\n') {
                output.pop_back();
            }
            output.append(static_cast<std::size_t>(ir_.new_lines_nodes_.count[node.index()]), '\n');
            continue;
        }
        auto const before_separator{output.size()};
        if (output.size() != start && output.back() != '\n') {
            output.append(static_cast<std::size_t>(newlines), '\n');
        }
        auto const before_node{output.size()};
        append_node(output, node, ctx);
        if (output.size() == before_node) {
            output.resize(before_separator);
        }
    }
}

auto CppRenderer::render(Syntax syntax, RenderContext const& ctx) -> std::string {
    state_.clear();
    return expression(syntax, ctx);
}
auto CppRenderer::expression(Syntax syntax, RenderContext const& ctx) -> std::string {
    std::string output;
    append_expr(output, syntax, ctx);
    return output;
}
auto CppRenderer::expr_precedence(Syntax syntax) -> int {
    return syntax.kind() == SyntaxKind::expression ? expr_precedence(syntax.expression()) : 12;
}
void CppRenderer::append_expr(
    std::string& output, Syntax syntax, RenderContext const& ctx, int minimum, bool equal) {
    auto const index{syntax.index()};
    auto append_arguments = [&](ir::Range range) {
        bool first{true};
        for (auto argument : ir_.exprs(range)) {
            if (!first) {
                output += ", ";
            }
            append_expr(output, argument, ctx, 1);
            first = false;
        }
    };
    switch (syntax.kind()) {
        case SyntaxKind::expression:
            append_expr(output, syntax.expression(), ctx, minimum, equal);
            break;
        case SyntaxKind::name:
            output += ir_.strings.view(ir_.named_exprs_.spelling[index]);
            break;
        case SyntaxKind::type_reference:
            append_type(output, CppTypeId{index}, ctx);
            break;
        case SyntaxKind::macro_invocation:
            append_expr(output, ir_.call_exprs_.callee[index], ctx);
            output += '(';
            append_arguments(ir_.call_exprs_.arguments[index]);
            output += ')';
            break;
        case SyntaxKind::pack_expansion:
            append_expr(output, ir_.pack_patterns_[index], ctx, 1);
            output += "...";
            break;
        case SyntaxKind::capture:
        case SyntaxKind::reference_capture:
            if (syntax.kind() == SyntaxKind::reference_capture) {
                output += '&';
            }
            output += ir_.strings.view(StringId{index});
            break;
        case SyntaxKind::binding_list: {
            output += '[';
            bool first{true};
            for (auto name : ir::range_values(ir_.binding_names_, ir_.binding_lists_[index])) {
                if (!first) {
                    output += ", ";
                }
                output += ir_.strings.view(name);
                first = false;
            }
            output += ']';
            break;
        }
        case SyntaxKind::template_id:
            append_expr(output, ir_.template_id_exprs_.callee[index], ctx, 12);
            output += '<';
            append_arguments(ir_.template_id_exprs_.arguments[index]);
            output += '>';
            break;
        case SyntaxKind::member_name:
            append_expr(output, ir_.member_exprs_.object[index], ctx, 12);
            output += ir_.member_exprs_.through_pointer[index] ? "->" : ".";
            output += ir_.strings.view(ir_.member_exprs_.member[index]);
            break;
        case SyntaxKind::scope_name:
            append_type(output, ir_.scope_name_owners_[index], ctx);
            output += "::";
            output += ir_.strings.view(ir_.scope_name_members_[index]);
            break;
        case SyntaxKind::initializer_list:
            output += '{';
            append_arguments(ir_.initializer_exprs_.elements[index]);
            output += '}';
            break;
        case SyntaxKind::designated_initializer:
            output += '.';
            output += ir_.strings.view(ir_.designated_initializers_.member[index]);
            output += " = ";
            append_expr(output, ir_.designated_initializers_.value[index], ctx);
            break;
        case SyntaxKind::authored_fragment:
            output += ir_.strings.view(ir_.authored_fragments_[index]);
            break;
        case SyntaxKind::invalid:
            std::unreachable();
    }
}

} // namespace lispb::codegen
