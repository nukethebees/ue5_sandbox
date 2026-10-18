#include <ioj/is_any.h>

#include <codegen/ast/access.h>
#include <codegen/ast/access_indentation.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/expr_kind.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_disposition_placement.h>
#include <codegen/ast/function_exception_kind.h>
#include <codegen/ast/function_implementation_kind.h>
#include <codegen/ast/function_kind.h>
#include <codegen/ast/function_opening_brace_placement.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_requires_placement.h>
#include <codegen/ast/function_template_placement.h>
#include <codegen/ast/function_trailing_return_placement.h>
#include <codegen/ast/include.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/ir/range.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_kind.h>
#include <codegen/ast/record_kind.h>
#include <codegen/ast/render_context.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/cpp_string.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen {

auto CppIR::signature(std::uint32_t index, bool record_scope) const -> std::string {
    auto const id{function_nodes_.function[index].value};
    auto const declaration{function_nodes_.declaration[index] != 0};
    auto const header{function_nodes_.is_header[index] != 0};
    auto const owner{function_nodes_.owner[index]};
    auto const format{functions_.formatting[id]};
    auto const implementation{functions_.implementation[id]};
    if (owner && implementation.kind() == FunctionImplementationKind::deleted) {
        throw std::invalid_argument{"Function '" + render(functions_.name[id]) +
                                    "' deleted function cannot be an out-of-line definition"};
    }
    auto const return_syntax{
        function_nodes_.return_syntax[index].value_or(functions_.return_syntax[id])};
    auto const receiver{functions_.receiver[id]};
    std::string output;
    if (functions_.flags[id].is_nodiscard()) {
        output += "[[nodiscard]] ";
    }
    if (functions_.flags[id].is_inline() && header && (owner || !record_scope)) {
        output += "inline ";
    }
    if (receiver == FunctionReceiver::static_ && !owner) {
        output += "static ";
    }
    if (functions_.flags[id].is_constexpr()) {
        output += "constexpr ";
    }
    auto const export_specifier{(declaration || header) ? functions_.export_specifier[id]
                                                        : IdentifierId{}};
    if (export_specifier.value != 0 && !format.export_after_return_type) {
        output += strings.view(export_specifier);
        output += ' ';
    }
    if (functions_.kind[id] == FunctionKind::ordinary) {
        output += return_syntax == ReturnSyntax::trailing
                    ? std::string_view{"auto"}
                    : types.spelling(functions_.return_type[id]);
        output += ' ';
    } else if (functions_.kind[id] == FunctionKind::explicit_constructor && !owner) {
        output += "explicit ";
    }
    if (export_specifier.value != 0 && format.export_after_return_type) {
        output += strings.view(export_specifier);
        output += ' ';
    }
    if (owner) {
        output += strings.view(*owner);
        output += "::";
    }
    append_expr(output, functions_.name[id]);
    output += '(';
    auto const parameters{functions_.parameters[id]};
    auto const parameter_end{parameters.begin + parameters.count};
    for (auto p{parameters.begin}; p < parameter_end; ++p) {
        if (p != parameters.begin) {
            output += ", ";
        }
        output += types.spelling(parameters_.type[parameter_references_[p].value]);
        auto const parameter_name{strings.view(parameters_.name[parameter_references_[p].value])};
        if (!parameter_name.empty()) {
            output += ' ';
            output += parameter_name;
        }
        if (auto const value{parameters_.default_value[parameter_references_[p].value]};
            value && (declaration || header)) {
            output += " = ";
            output += strings.view(*value);
        }
    }
    output += ')';
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
    auto const exception{functions_.exception_specification[id]};
    switch (exception.kind()) {
        case FunctionExceptionKind::potentially_throwing:
            break;
        case FunctionExceptionKind::noexcept_:
            output += " noexcept";
            break;
        case FunctionExceptionKind::conditional:
            output += " noexcept(";
            append_expr(output, conditional_noexcept_[exception.index()]);
            output += ')';
            break;
    }
    if (functions_.kind[id] == FunctionKind::ordinary && return_syntax == ReturnSyntax::trailing) {
        output += format.trailing_return_placement == FunctionTrailingReturnPlacement::next_line
                    ? "\n    -> "
                    : " -> ";
        output += types.spelling(functions_.return_type[id]);
    }
    if (auto const clause{functions_.requires_clause[id]}) {
        auto const value{render(clause)};
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
    if (!declaration && implementation.kind() == FunctionImplementationKind::body) {
        auto const initializers{function_bodies_.initializers[implementation.index()]};
        auto const end{initializers.begin + initializers.count};
        for (auto m{initializers.begin}; m < end; ++m) {
            output += m == initializers.begin ? "\n    : " : "\n    , ";
            output += strings.view(initializers_.member[initializer_references_[m].value]);
            output += '{';
            output += strings.view(initializers_.value[initializer_references_[m].value]);
            output += '}';
        }
    }
    if (auto const declaration{functions_.template_declaration[id]}) {
        std::string prefix;
        append_template(prefix, declaration);
        prefix += format.template_placement == FunctionTemplatePlacement::same_line ? " " : "\n";
        output.insert(0, prefix);
    }
    if (ioj::is_any(implementation.kind(),
                    {FunctionImplementationKind::defaulted, FunctionImplementationKind::deleted})) {
        output += format.disposition_placement == FunctionDispositionPlacement::next_line ? "\n= "
                                                                                          : " = ";
        output +=
            implementation.kind() == FunctionImplementationKind::defaulted ? "default" : "delete";
    }
    return output;
}

void CppIR::append_nodes(std::string& output,
                         std::span<NodeId const> values,
                         RenderContext const& context,
                         int newlines) const {
    auto const start{output.size()};
    for (auto const node : values) {
        if (node.kind() == NodeKind::new_lines) {
            while (output.size() > start && output.back() == '\n') {
                output.pop_back();
            }
            output.append(static_cast<std::size_t>(new_lines_nodes_.count[node.index()]), '\n');
            continue;
        }
        auto const before_separator{output.size()};
        if (output.size() != start && output.back() != '\n') {
            output.append(static_cast<std::size_t>(newlines), '\n');
        }
        auto const before_node{output.size()};
        append_node(output, node, context);
        if (output.size() == before_node) {
            output.resize(before_separator);
        }
    }
}
void CppIR::append_node(std::string& output, NodeId node, RenderContext const& context) const {
    auto const i{node.index()};
    auto indent = [&] { ir::append_indent(output, context); };
    auto write = [&](std::string_view text) { ir::append_indented(output, text, context); };
    auto close = [&](std::string_view text) {
        output += '\n';
        indent();
        output += text;
    };
    auto type = [&](CppTypeId id) { output += types.spelling(id); };
    auto text = [&](StringId id) { output += strings.view(id); };
    auto expr = [&](Expr id) { append_expr(output, id); };
    auto initialize = [&](Expr value) {
        auto const direct_list{value.kind() == ExprKind::initializer_list &&
                               !initializer_exprs_.type[value.index()]};
        if (!direct_list) {
            output += '{';
        }
        expr(value);
        if (!direct_list) {
            output += '}';
        }
    };
    switch (node.kind()) {
        case NodeKind::raw:
            write(strings.view(raw_nodes_.text[i]));
            break;
        case NodeKind::raw_lines: {
            auto const range{raw_lines_nodes_.lines[i]};
            auto const end{range.begin + range.count};
            for (auto line{range.begin}; line < end; ++line) {
                if (line != range.begin) {
                    output += '\n';
                }
                write(strings.view(raw_lines_[line]));
            }
            break;
        }
        case NodeKind::line_comment:
            write("// " + std::string{strings.view(line_comment_nodes_.text[i])});
            break;
        case NodeKind::line_directive:
            output += "#line ";
            output += std::to_string(line_directive_nodes_.line[i]);
            output += " \"";
            append_escaped_cpp_string(output, strings.view(line_directive_nodes_.path[i]));
            output += '"';
            break;
        case NodeKind::block_comment:
            write("/* " + std::string{strings.view(block_comment_nodes_.text[i])} + " */");
            break;
        case NodeKind::block:
            write("{");
            output += '\n';
            append_nodes(output, nodes(block_nodes_.children[i]), context.indent(), 1);
            close("}");
            break;
        case NodeKind::if_stmt:
            indent();
            output += "if (";
            expr(if_stmt_nodes_.condition[i]);
            output += ") {\n";
            append_nodes(output, nodes(if_stmt_nodes_.then_children[i]), context.indent(), 1);
            close("}");
            if (auto const otherwise{if_stmt_nodes_.else_children[i]}) {
                output += " else {\n";
                append_nodes(output, nodes(*otherwise), context.indent(), 1);
                close("}");
            }
            break;
        case NodeKind::for_stmt: {
            indent();
            output += "for (";
            append_node(output, for_stmt_nodes_.initializer[i], {});
            output += ' ';
            expr(for_stmt_nodes_.condition[i]);
            output += "; ";
            expr(for_stmt_nodes_.increment[i]);
            output += ") {\n";
            append_nodes(output, nodes(for_stmt_nodes_.children[i]), context.indent(), 1);
            close("}");
            break;
        }
        case NodeKind::switch_stmt: {
            indent();
            output += "switch (";
            expr(switch_stmt_nodes_.condition[i]);
            output += ") {";
            auto const range{switch_stmt_nodes_.cases[i]};
            auto const end{range.begin + range.count};
            for (auto c{range.begin}; c < end; ++c) {
                output += '\n';
                indent();
                if (auto const label{cases_.label[case_references_[c].value]}) {
                    output += "case ";
                    expr(*label);
                } else {
                    output += "default";
                }
                output += ": {\n";
                append_nodes(
                    output, nodes(cases_.body[case_references_[c].value]), context.indent(), 1);
                close("}");
            }
            close("}");
            break;
        }
        case NodeKind::break_stmt:
            write("break;");
            break;
        case NodeKind::expr_stmt:
            indent();
            expr(expr_stmt_nodes_.expr[i]);
            output += ';';
            break;
        case NodeKind::return_stmt:
            indent();
            output += "return";
            if (auto const value{return_stmt_nodes_.expr[i]}) {
                output += ' ';
                expr(*value);
            }
            output += ';';
            break;
        case NodeKind::assignment_stmt:
            indent();
            expr(assignment_stmt_nodes_.target[i]);
            output += " = ";
            expr(assignment_stmt_nodes_.value[i]);
            output += ';';
            break;
        case NodeKind::variable_declaration_stmt:
            indent();
            type(variable_declaration_stmt_nodes_.type[i]);
            output += ' ';
            text(variable_declaration_stmt_nodes_.name[i]);
            if (auto const initializer{variable_declaration_stmt_nodes_.initializer[i]}) {
                initialize(*initializer);
            } else {
                output += "{}";
            }
            output += ';';
            break;
        case NodeKind::static_assert_:
            indent();
            output += "static_assert(";
            text(static_assert_nodes_.condition[i]);
            if (auto const message{strings.view(static_assert_nodes_.message[i])};
                !message.empty()) {
                output += ", \"";
                output += escape_cpp_string(message);
                output += '"';
            }
            output += ");";
            break;
        case NodeKind::new_lines:
            output.append(static_cast<std::size_t>(new_lines_nodes_.count[i]), '\n');
            break;
        case NodeKind::access_specifier: {
            auto const begin{output.size()};
            indent();
            if (access_specifier_nodes_.indentation[i] == AccessIndentation::outdented &&
                output.size() - begin >= 2) {
                output.resize(output.size() - 2);
            }
            switch (access_specifier_nodes_.access[i]) {
                case Access::public_:
                    output += "public:";
                    break;
                case Access::protected_:
                    output += "protected:";
                    break;
                case Access::private_:
                    output += "private:";
                    break;
            }
            break;
        }
        case NodeKind::include: {
            auto const path{strings.view(include_nodes_.path[i])};
            auto const system{include_is_system(path, include_nodes_.style[i])};
            indent();
            output += "#include ";
            output += system ? '<' : '"';
            output += path;
            output += system ? '>' : '"';
            break;
        }
        case NodeKind::include_dependencies: {
            bool first_group{true};
            for (auto const& group : context.include_groups) {
                if (!first_group) {
                    output += "\n\n";
                }
                bool first{true};
                for (auto const& include : group) {
                    if (!first) {
                        output += '\n';
                    }
                    indent();
                    output += "#include ";
                    auto const system{include_is_system(include)};
                    output += system ? '<' : '"';
                    output += include;
                    output += system ? '>' : '"';
                    first = false;
                }
                first_group = false;
            }
            break;
        }
        case NodeKind::forward_declaration:
            if (auto const declaration{forward_declaration_nodes_.template_declaration[i]}) {
                indent();
                append_template(output, declaration);
                output += '\n';
            }
            indent();
            output += record_keyword(forward_declaration_nodes_.kind[i]);
            output += ' ';
            text(forward_declaration_nodes_.name[i]);
            output += ';';
            break;
        case NodeKind::friend_declaration:
            if (auto const declaration{friend_declaration_nodes_.template_declaration[i]}) {
                indent();
                append_template(output, declaration);
                output += '\n';
            }
            indent();
            output += "friend ";
            if (auto const friend_type{friend_declaration_nodes_.type[i]}; friend_type.value != 0) {
                type(friend_type);
            } else {
                output += record_keyword(friend_declaration_nodes_.kind[i]);
                output += ' ';
                text(friend_declaration_nodes_.name[i]);
            }
            output += ';';
            break;
        case NodeKind::using_declaration:
            if (auto const declaration{using_declaration_nodes_.template_declaration[i]}) {
                indent();
                append_template(output, declaration);
                output += '\n';
            }
            indent();
            output += "using ";
            text(using_declaration_nodes_.name[i]);
            output += " = ";
            type(using_declaration_nodes_.type[i]);
            output += ';';
            break;
        case NodeKind::instance_member:
            indent();
            type(instance_members_.type[i]);
            output += ' ';
            output += strings.view(instance_members_.name[i]);
            if (auto const initializer{instance_members_.initializer[i]}) {
                initialize(initializer);
            }
            output += ';';
            break;
        case NodeKind::static_member: {
            if (auto const parameters{static_members_.template_declaration[i]}) {
                indent();
                append_template(output, parameters);
                output += '\n';
            }
            indent();
            if (static_members_.is_inline[i]) {
                output += "inline ";
            }
            output += "static ";
            type(static_members_.type[i]);
            output += ' ';
            output += strings.view(static_members_.name[i]);
            if (auto const initializer{static_members_.initializer[i]}) {
                initialize(initializer);
            }
            output += ';';
            break;
        }
        case NodeKind::constexpr_member: {
            if (auto const parameters{constexpr_members_.template_declaration[i]}) {
                indent();
                append_template(output, parameters);
                output += '\n';
            }
            indent();
            if (constexpr_members_.is_inline[i]) {
                output += "inline ";
            }
            output += "static constexpr ";
            type(constexpr_members_.type[i]);
            output += ' ';
            output += strings.view(constexpr_members_.name[i]);
            initialize(constexpr_members_.initializer[i]);
            output += ';';
            break;
        }
        case NodeKind::function: {
            auto const id{function_nodes_.function[i].value};
            write(signature(i, context.record_scope));
            auto const implementation{functions_.implementation[id]};
            if (function_nodes_.declaration[i] ||
                implementation.kind() != FunctionImplementationKind::body) {
                output += ';';
                break;
            }
            auto const body{
                block_nodes_.children[function_bodies_.block[implementation.index()].index()]};
            auto const format{functions_.formatting[id]};
            if (format.body_layout == FunctionBodyLayout::compact) {
                auto body_context{context};
                body_context.indent_level = 0;
                output += " { ";
                append_nodes(output, nodes(body), body_context, 1);
                output += " }";
            } else {
                if (format.opening_brace_placement ==
                    FunctionOpeningBracePlacement::separate_line) {
                    close("{");
                } else {
                    output += " {";
                }
                output += '\n';
                append_nodes(output, nodes(body), context.indent(), 1);
                close("}");
            }
            break;
        }
        case NodeKind::enum_: {
            indent();
            output += "enum";
            if (enum_nodes_.scoped[i]) {
                output += " class";
            }
            if (auto const specifier{enum_nodes_.export_specifier[i]}; specifier.value != 0) {
                output += ' ';
                output += strings.view(specifier);
            }
            output += ' ';
            text(enum_nodes_.name[i]);
            if (auto const underlying{enum_nodes_.underlying_type[i]};
                underlying.value != 0 && !types.spelling(underlying).empty()) {
                output += " : ";
                type(underlying);
            }
            output += " {\n";
            auto const range{enum_nodes_.values[i]};
            auto const end{range.begin + range.count};
            for (auto e{range.begin}; e < end; ++e) {
                ir::append_indented(
                    output,
                    strings.view(enumerators_.name[enumerator_references_[e].value]),
                    context.indent());
                if (auto const initializer{
                        enumerators_.initializer[enumerator_references_[e].value]}) {
                    output += " = ";
                    text(*initializer);
                }
                if (auto const annotation{
                        enumerators_.annotation[enumerator_references_[e].value]}) {
                    output += ' ';
                    text(*annotation);
                }
                output += ',';
                if (e + 1 < end) {
                    output += '\n';
                }
            }
            if (range.count == 0) {
                output += ',';
            }
            close("};");
            break;
        }
        case NodeKind::struct_: {
            if (auto const parameters{struct_nodes_.template_declaration[i]}) {
                indent();
                append_template(output, parameters);
                output += '\n';
                if (auto const clause{struct_nodes_.requires_clause[i]}) {
                    indent();
                    output += "    requires ";
                    expr(clause);
                    output += '\n';
                }
            }
            indent();
            output += record_keyword(struct_nodes_.record_kind[i]);
            if (auto const specifier{struct_nodes_.export_specifier[i]}; specifier.value != 0) {
                output += ' ';
                output += strings.view(specifier);
            }
            output += ' ';
            expr(struct_nodes_.name[i]);
            bool first{true};
            for (auto const base : type_list(struct_nodes_.bases[i])) {
                output += first ? " : " : ", ";
                type(base);
                first = false;
            }
            auto member_context{context.indent()};
            member_context.record_scope = true;
            output += " {\n";
            append_nodes(output, nodes(struct_nodes_.children[i]), member_context, 2);
            close("};");
            break;
        }
        case NodeKind::namespace_: {
            auto const name{strings.view(namespace_nodes_.name[i])};
            indent();
            output += "namespace ";
            output += name;
            output += name.empty() ? "{\n" : " {\n";
            append_nodes(output, nodes(namespace_nodes_.children[i]), context, 2);
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
auto CppIR::render(NodeId node, RenderContext const& context) const -> std::string {
    std::string output;
    append_node(output, node, context);
    return output;
}
auto CppIR::render_nodes(std::span<NodeId const> values,
                         RenderContext const& context,
                         int newlines) const -> std::string {
    std::string output;
    append_nodes(output, values, context, newlines);
    return output;
}

void CppIR::for_each_child(NodeId node, std::function<void(NodeId)> const& visit) const {
    auto visit_range = [&](ir::Range range) {
        for (auto const child : nodes(range)) {
            visit(child);
        }
    };
    auto const i{node.index()};
    switch (node.kind()) {
        case NodeKind::block:
            visit_range(block_nodes_.children[i]);
            break;
        case NodeKind::struct_:
            visit_range(struct_nodes_.children[i]);
            break;
        case NodeKind::namespace_:
            visit_range(namespace_nodes_.children[i]);
            break;
        case NodeKind::function: {
            auto const implementation{functions_.implementation[function_nodes_.function[i].value]};
            if (!function_nodes_.declaration[i] &&
                implementation.kind() == FunctionImplementationKind::body) {
                visit_range(
                    block_nodes_.children[function_bodies_.block[implementation.index()].index()]);
            }
            break;
        }
        case NodeKind::if_stmt:
            visit_range(if_stmt_nodes_.then_children[i]);
            if (auto const otherwise{if_stmt_nodes_.else_children[i]}) {
                visit_range(*otherwise);
            }
            break;
        case NodeKind::for_stmt:
            visit(for_stmt_nodes_.initializer[i]);
            visit_range(for_stmt_nodes_.children[i]);
            break;
        case NodeKind::switch_stmt: {
            auto const range{switch_stmt_nodes_.cases[i]};
            auto const end{range.begin + range.count};
            for (auto c{range.begin}; c < end; ++c) {
                visit_range(cases_.body[case_references_[c].value]);
            }
            break;
        }
        default:
            break;
    }
}
void CppIR::collect_dependencies(NodeId node, std::vector<TypeDependencyId>& result) const {
    auto append = [&](std::span<TypeDependencyId const> values) {
        result.insert(result.end(), values.begin(), values.end());
    };
    auto type = [&](CppTypeId id) { append(types.dependencies(id)); };
    auto const i{node.index()};
    switch (node.kind()) {
        case NodeKind::raw:
            append(dependency_list(raw_nodes_.dependencies[i]));
            break;
        case NodeKind::raw_lines:
            append(dependency_list(raw_lines_nodes_.dependencies[i]));
            break;
        case NodeKind::expr_stmt:
            append(dependency_list(expr_stmt_nodes_.dependencies[i]));
            collect_dependencies(expr_stmt_nodes_.expr[i], result);
            break;
        case NodeKind::return_stmt:
            append(dependency_list(return_stmt_nodes_.dependencies[i]));
            if (auto const value{return_stmt_nodes_.expr[i]}) {
                collect_dependencies(*value, result);
            }
            break;
        case NodeKind::assignment_stmt:
            append(dependency_list(assignment_stmt_nodes_.dependencies[i]));
            collect_dependencies(assignment_stmt_nodes_.target[i], result);
            collect_dependencies(assignment_stmt_nodes_.value[i], result);
            break;
        case NodeKind::variable_declaration_stmt:
            type(variable_declaration_stmt_nodes_.type[i]);
            if (auto const initializer{variable_declaration_stmt_nodes_.initializer[i]}) {
                collect_dependencies(*initializer, result);
            }
            break;
        case NodeKind::if_stmt:
            collect_dependencies(if_stmt_nodes_.condition[i], result);
            break;
        case NodeKind::for_stmt:
            collect_dependencies(for_stmt_nodes_.condition[i], result);
            collect_dependencies(for_stmt_nodes_.increment[i], result);
            break;
        case NodeKind::switch_stmt: {
            collect_dependencies(switch_stmt_nodes_.condition[i], result);
            auto const range{switch_stmt_nodes_.cases[i]};
            auto const end{range.begin + range.count};
            for (auto c{range.begin}; c < end; ++c) {
                if (auto const label{cases_.label[case_references_[c].value]}) {
                    collect_dependencies(*label, result);
                }
            }
            break;
        }
        case NodeKind::forward_declaration:
            collect_dependencies(forward_declaration_nodes_.template_declaration[i], result);
            break;
        case NodeKind::friend_declaration:
            collect_dependencies(friend_declaration_nodes_.template_declaration[i], result);
            if (auto const friend_type{friend_declaration_nodes_.type[i]}; friend_type.value != 0) {
                type(friend_type);
            }
            break;
        case NodeKind::using_declaration:
            collect_dependencies(using_declaration_nodes_.template_declaration[i], result);
            type(using_declaration_nodes_.type[i]);
            break;
        case NodeKind::instance_member:
            type(instance_members_.type[i]);
            if (auto const value{instance_members_.initializer[i]}) {
                collect_dependencies(value, result);
            }
            break;
        case NodeKind::static_member:
            collect_dependencies(static_members_.template_declaration[i], result);
            type(static_members_.type[i]);
            if (auto const value{static_members_.initializer[i]}) {
                collect_dependencies(value, result);
            }
            break;
        case NodeKind::constexpr_member:
            collect_dependencies(constexpr_members_.template_declaration[i], result);
            type(constexpr_members_.type[i]);
            collect_dependencies(constexpr_members_.initializer[i], result);
            break;
        case NodeKind::function: {
            auto const id{function_nodes_.function[i].value};
            collect_dependencies(functions_.name[id], result);
            collect_dependencies(functions_.template_declaration[id], result);
            if (auto const clause{functions_.requires_clause[id]}) {
                collect_dependencies(clause, result);
            }
            if (functions_.return_type[id].value != 0) {
                type(functions_.return_type[id]);
            }
            append(dependency_list(functions_.dependencies[id]));
            if (auto const exception{functions_.exception_specification[id]};
                exception.kind() == FunctionExceptionKind::conditional) {
                collect_dependencies(conditional_noexcept_[exception.index()], result);
            }
            auto const range{functions_.parameters[id]};
            auto const end{range.begin + range.count};
            for (auto p{range.begin}; p < end; ++p) {
                type(parameters_.type[parameter_references_[p].value]);
            }
            break;
        }
        case NodeKind::enum_:
            if (enum_nodes_.underlying_type[i].value != 0) {
                type(enum_nodes_.underlying_type[i]);
            }
            break;
        case NodeKind::struct_:
            collect_dependencies(struct_nodes_.name[i], result);
            collect_dependencies(struct_nodes_.template_declaration[i], result);
            if (auto const clause{struct_nodes_.requires_clause[i]}) {
                collect_dependencies(clause, result);
            }
            append(dependency_list(struct_nodes_.dependencies[i]));
            for (auto const base : type_list(struct_nodes_.bases[i])) {
                type(base);
            }
            break;
        default:
            break;
    }
    for_each_child(node, [&](NodeId child) { collect_dependencies(child, result); });
}
auto CppIR::render(CppFileId file) const -> std::string {
    auto const row{files.index(file)};
    auto const file_nodes{files.nodes(file)};
    auto const flags{files.flags[row]};
    std::vector<TypeDependencyId> dependencies;
    for (auto const node : file_nodes) {
        collect_dependencies(node, dependencies);
    }
    std::set<std::string> paths;
    for (auto const dependency : dependencies) {
        if (auto const header{types.dependency_header(dependency)}) {
            paths.emplace(*header);
        }
    }
    std::vector<std::vector<std::string>> include_groups;
    for (auto const prefix_id : files.include_order(file)) {
        auto const prefix{strings.view(prefix_id)};
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
    RenderContext const context{.include_groups = include_groups};
    std::string output;
    if (flags.clang_format_off()) {
        output += "// clang-format off\n";
    }
    output += "// This file is autogenerated. Do not edit by hand.\n";
    if (files.source[row].value != 0) {
        output += "// Source: ";
        output += strings.view(files.source[row]);
    } else {
        output += "// Edit lispb/schema and regenerate it instead.";
    }
    for (auto const prologue : files.prologue(file)) {
        output += "\n\n";
        output += strings.view(prologue);
    }
    if (flags.pragma_once()) {
        output += "\n\n#pragma once";
    }
    if (!file_nodes.empty()) {
        output += "\n\n";
        append_nodes(output, file_nodes, context, 1);
    }
    for (auto const epilogue : files.epilogue(file)) {
        output += "\n\n";
        output += strings.view(epilogue);
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

} // namespace lispb::codegen
