#include "soa_api.h"

#include "lowered_soa.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_model.h"
#include "soa_body.h"
#include "soa_receiver.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/function_schemas.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/physical_type_use.h>
#include <codegen/schema/schema_range.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_member_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::detail {
auto logical_column_access(LoweringContext& context,
                           DeclarationHandle schema,
                           std::span<std::string const> const path,
                           std::string receiver) -> std::string {
    if (path.empty()) {
        throw std::invalid_argument{"SOA column reference cannot be empty"};
    }
    auto current{schema};
    for (std::size_t index{}; index < path.size(); ++index) {
        if (!receiver.empty() && !receiver.ends_with("->")) {
            receiver += '.';
        }
        auto const members{context.declarations.items(context.declarations.soas.members[current])};
        auto const found{std::ranges::find(members, path[index], [&](auto id) {
            return context.declarations.strings().view(context.declarations.soa_members.name[id]);
        })};
        if (found == members.end()) {
            throw std::invalid_argument{"SOA '" +
                                        context.text(context.declarations.soas.name[current]) +
                                        "' has no column '" + path[index] + "'"};
        }
        auto const member{*found};
        bool const nested{context.declarations.soa_members.kind[member] == SoaMemberKind::nested};
        receiver += nested ? "view_" : "";
        receiver += path[index];
        receiver += "()";
        if (index + 1 < path.size()) {
            if (!nested) {
                throw std::invalid_argument{"SOA column path requires a nested column: " +
                                            path[index]};
            }
            current = context.for_declaration(current).nested_declaration(member);
        }
    }
    return receiver;
}

namespace {
auto has_custom_row_function(LoweringContext& context,
                             DeclarationHandle schema,
                             SoaReceiver receiver,
                             std::string_view name) -> bool {
    auto const& declarations{context.declarations};
    auto const& soas{declarations.soas};
    auto const row{soas.index(schema)};
    auto const functions{receiver == SoaReceiver::owner ? soas.functions[row]
                                                        : soas.mutable_view_functions[row]};
    return std::ranges::any_of(declarations.items(functions), [&](auto function) {
        return context.text(declarations.functions.name[function]) == name;
    });
}

auto compact_row_api(LoweringContext& context,
                     DeclarationHandle schema,
                     SoaReceiver receiver,
                     CppTypeId base) -> Nodes {
    auto& ir{context.ir};
    auto const& declarations{context.declarations};
    auto const& soas{declarations.soas};
    auto const& members{declarations.soa_members};
    NodeListBuilder result{ir};
    auto const owner{receiver == SoaReceiver::owner};
    auto const mutable_view{receiver == SoaReceiver::mutable_view};
    auto custom = [&](std::string_view name) {
        return has_custom_row_function(context, schema, receiver, name);
    };
    auto emit = [&](std::string_view name,
                    std::vector<FunctionParameterId> parameters,
                    Nodes body) {
        auto const function{ir.function(name, name == "set" ? "void" : "size_type")};
        if (name == "set") {
            parameters.insert(parameters.begin(), ir.parameter("size_type", "index"));
            ir.set_function_receiver(
                function, mutable_view ? FunctionReceiver::const_ : FunctionReceiver::unqualified);
        }
        ir.set_function_parameters(function, parameters);
        ir.set_function_body(function, body);
        ir.function_flags(function).set_define_in_header(true);
        ir.function_flags(function).set_is_inline(true);
        result.add(ir.header_function(function)).new_lines(1);
    };

    if (owner) {
        for (auto member : declarations.items(soas.members[schema])) {
            auto const row{members.index(member)};
            auto const name{(members.kind[row] == SoaMemberKind::nested ? "view_" : "") +
                            context.text(members.name[row])};
            result
                .add(ir.raw("auto " + name + "(this auto& self) { return self.get_view()." + name +
                            "(); }"))
                .new_lines(1);
        }
    }
    if ((!owner && !mutable_view) || custom("set")) {
        return result.build();
    }
    if (owner) {
        result.add(ir.using_declaration(ir.strings.intern("RowOperations"), base)).new_lines(1);
    }
    result.add(ir.raw(owner ? "using RowOperations::set;" : "using Base::set;")).new_lines(1);
    if (owner && !custom("add")) {
        result.add(ir.raw("using RowOperations::add;")).new_lines(1);
    }

    std::vector<CppTypeId> parameter_types;
    std::vector<Expr> equivalent_arguments;
    auto mapped{soas.equivalent_type[schema].has_value()};
    auto collect = [&](auto&& self, DeclarationHandle current, std::string const& value) -> void {
        for (auto member : declarations.items(soas.members[current])) {
            auto const row{members.index(member)};
            auto component{value};
            if (members.equivalent_member[row]) {
                component += "." + context.text(*members.equivalent_member[row]);
                if (members.equivalent_accessor[row]) {
                    component += "()";
                }
            } else {
                mapped = false;
            }
            if (members.kind[row] == SoaMemberKind::nested) {
                self(self, context.for_declaration(current).nested_declaration(member), component);
                continue;
            }
            auto const type{context.for_declaration(current).resolve(members.type[row])};
            parameter_types.push_back(ir.types.decorate(type, "", " const"));
            equivalent_arguments.push_back(ir.raw_expr(component));
        }
    };
    collect(collect, schema, "value");
    auto can_assign_equivalent = [&](auto&& self, DeclarationHandle current) -> bool {
        if (!soas.equivalent_type[current]) {
            return false;
        }
        for (auto const member : declarations.items(soas.members[current])) {
            auto const row{members.index(member)};
            if (!members.equivalent_member[row]) {
                return false;
            }
            if (members.kind[row] == SoaMemberKind::nested &&
                !self(self, context.for_declaration(current).nested_declaration(member))) {
                return false;
            }
        }
        return true;
    };
    std::vector<FunctionParameterId> logical_parameters;
    std::vector<Expr> logical_arguments{ir.named("index")};
    NodeListBuilder logical_assignments{ir};
    logical_assignments.add(ir.raw("assert(index < num());"));
    auto collect_logical =
        [&](auto&& self, DeclarationHandle current, std::vector<std::string> const& path) -> void {
        for (auto const member : declarations.items(soas.members[current])) {
            auto const row{members.index(member)};
            auto column{path};
            column.push_back(context.text(members.name[row]));
            auto member_context{context.for_declaration(current)};
            auto type_reference{members.type[row]};
            bool const nested{members.kind[row] == SoaMemberKind::nested};
            if (nested) {
                auto const child{member_context.nested_declaration(member)};
                if (!can_assign_equivalent(can_assign_equivalent, child)) {
                    self(self, child, column);
                    continue;
                }
                member_context.module = context.for_declaration(child).module;
                type_reference = *soas.equivalent_type[child];
            }
            auto const name{"new_" + join(column, "_")};
            auto const type{member_context.resolve(type_reference)};
            logical_parameters.push_back(ir.parameter(ir.types.decorate(type, "", " const"), name));
            logical_arguments.push_back(ir.named(name));
            auto const access{
                logical_column_access(context, schema, column, owner ? "get_view()" : "this->")};
            logical_assignments.add(ir.raw(nested ? access + ".set(index, " + name + ");"
                                                  : access + "[index] = " + name + ";"));
        }
    };
    collect_logical(collect_logical, schema, {});
    auto const same_parameters{std::ranges::equal(
        parameter_types,
        logical_parameters,
        {},
        [&](auto type) { return ir.types.spelling(type); },
        [&](auto parameter) { return ir.types.spelling(ir.parameter_type(parameter)); })};
    if (!same_parameters) {
        emit("set", logical_parameters, logical_assignments.build());
        if (owner && !custom("add")) {
            emit("add",
                 logical_parameters,
                 {ir.raw("auto const index{num()};\nadd_uninitialised(1);"),
                  ir.expr_stmt(ir.call(ir.named("set"), logical_arguments)),
                  ir.return_stmt(ir.named("index"))});
        }
    }
    if (mapped) {
        auto const equivalent{context.resolve(*soas.equivalent_type[schema])};
        auto const value{ir.parameter(ir.types.decorate(equivalent, "", " const"), "value")};
        auto set_arguments{equivalent_arguments};
        set_arguments.insert(set_arguments.begin(), ir.named("index"));
        emit("set", {value}, {ir.expr_stmt(ir.call(ir.named("set"), set_arguments))});
        if (owner && !custom("add")) {
            emit("add", {value}, {ir.return_stmt(ir.call(ir.named("add"), equivalent_arguments))});
        }
    }
    return result.build();
}

auto expand_columns(LoweringContext& context,
                    std::string_view const body,
                    DeclarationHandle schema,
                    SoaReceiver const receiver) -> std::string {
    std::string result;
    result.reserve(body.size());
    std::size_t copied{};
    scan_soa_columns(body,
                     context.declarations.strings().view(context.declarations.soas.name[schema]),
                     [&](std::size_t const begin,
                         std::size_t const end,
                         std::span<std::string const> const path) {
                         result.append(body.substr(copied, begin - copied));
                         auto const prefix{receiver == SoaReceiver::owner ? "get_view()"
                                                                          : "this->"};
                         result += logical_column_access(context, schema, path, prefix);
                         copied = end;
                     });
    result.append(body.substr(copied));
    return result;
}
}

auto soa_row_operations(LoweringContext& context,
                        DeclarationHandle schema,
                        SoaReceiver receiver,
                        std::string const& type_name,
                        CppTypeId base) -> CppTypeId {
    if (receiver == SoaReceiver::const_view ||
        has_custom_row_function(context, schema, receiver, "set")) {
        return base;
    }
    auto& types{context.ir.types};
    auto const operations{receiver == SoaReceiver::owner ? "OwnerRowOperations"
                                                         : "ViewRowOperations"};
    return types.template_type(
        types.named("ioj::soa_storage_detail::" + std::string{operations},
                    {types.dependency(operations, "ioj/single_allocation/view.h")}),
        {types.named(type_name),
         types.named(schema_namespace(context, schema) + "::Layout::column_types"),
         base});
}

auto lower_soa_api(LoweringContext& parent_context,
                   DeclarationHandle schema,
                   SoaReceiver const receiver,
                   std::string const& type_name,
                   CppTypeId base) -> LoweredSoa {
    auto context{parent_context.for_declaration(schema)};
    auto& ir{context.ir};
    NodeListBuilder header{ir};
    NodeListBuilder source{ir};
    header.append(compact_row_api(context, schema, receiver, base));
    for (auto const alias :
         context.declarations.items(context.declarations.soas.using_declarations[schema])) {
        header.add(ir.raw("using " + context.text(alias) + ";")).new_lines(1);
    }
    auto append = [&](SchemaRange<FunctionSchemaId> const functions) {
        for (auto const function_id : context.declarations.items(functions)) {
            auto const function{function_id};
            auto const& function_schemas{context.declarations.functions};
            auto const row{function_schemas.index(function)};
            auto const body{expand_columns(
                context,
                join_lines(context, context.declarations.functions.body_lines[function]),
                schema,
                receiver)};
            auto spec{lower_schema_function(context, function, body)};
            if (auto const appended{function_schemas.append_columns[row]}) {
                auto const columns{context.declarations.items(*appended)};
                auto const parameters{context.declarations.items(function_schemas.parameters[row])};
                auto const& parameter_schemas{context.declarations.parameters};
                std::string index_name{"row"};
                while (std::ranges::any_of(parameters, [&](auto parameter) {
                    return context.text(
                               parameter_schemas.name[parameter_schemas.index(parameter)]) ==
                           index_name;
                })) {
                    index_name += '_';
                }
                auto const index{ir.named(index_name)};
                NodeListBuilder append_body{ir};
                append_body
                    .add(ir.variable("auto const", index_name, ir.call(ir.named("this->num"))))
                    .add(ir.expr_stmt(ir.call(ir.named("this->add_defaulted"), {ir.literal("1")})));
                auto const count{columns.size()};
                for (std::size_t column{}; column < count; ++column) {
                    append_body.add(ir.assignment(
                        ir.subscript(ir.call(ir.member_access(ir.call(ir.named("this->get_view")),
                                                              context.text(columns[column]))),
                                     index),
                        ir.named(
                            parameter_schemas.name[parameter_schemas.index(parameters[column])])));
                }
                auto const return_type{ir.function_return_type(spec)};
                auto const spelling{ir.types.spelling(return_type)};
                append_body.add(ir.return_stmt(spelling == "auto" || spelling == "decltype(auto)"
                                                   ? index
                                                   : ir.init_list({index}, return_type)));
                ir.set_function_body(spec, append_body.build());
                ir.function_flags(spec).set_define_in_header(
                    !function_schemas.definition_in_source[row]);
            }
            if (context.declarations.functions.definition_in_source[function]) {
                auto const id{spec};
                auto const use_trailing_return{
                    ir.function_return_syntax(spec) == ReturnSyntax::direct &&
                    ir.types.spelling(ir.function_return_type(spec)) != "auto" &&
                    ir.types.spelling(ir.function_return_type(spec)) != "decltype(auto)"};
                header.add(ir.declaration(id)).new_lines(1);
                source
                    .add(ir.definition(id,
                                       ir.types.intern(type_name),
                                       use_trailing_return ? ReturnSyntax::trailing
                                                           : ir.function_return_syntax(spec)))
                    .new_lines(2);
            } else {
                ir.function_flags(spec).set_define_in_header(
                    ir.function_flags(spec).define_in_header() ||
                    !context.declarations.functions.body_lines[function].empty());
                ir.function_flags(spec).set_is_inline(ir.function_flags(spec).define_in_header());
                header.add(ir.header_function(spec)).new_lines(1);
            }
        }
    };
    if (receiver == SoaReceiver::owner) {
        append(context.declarations.soas.functions[schema]);
    } else {
        append(context.declarations.soas.const_view_functions[schema]);
        if (receiver == SoaReceiver::mutable_view) {
            append(context.declarations.soas.mutable_view_functions[schema]);
        }
    }
    if (context.declarations.soas.equivalent_type[schema]) {
        auto const equivalent{context.resolve(*context.declarations.soas.equivalent_type[schema])};
        header.add(ir.using_declaration(ir.strings.intern("equivalent_type"), equivalent))
            .new_lines(1);
        std::string expression;
        if (receiver == SoaReceiver::owner) {
            expression = "get_const_view()[index]";
        } else {
            auto const constructor{context.declarations.soas.equivalent_constructor[schema]};
            expression = constructor ? context.text(*constructor) + "(" : "equivalent_type{";
            bool first{true};
            for (auto const member_id :
                 context.declarations.items(context.declarations.soas.members[schema])) {
                auto const member{member_id};
                if (!first) {
                    expression += ", ";
                }
                first = false;
                std::vector<std::string> const path{
                    context.text(context.declarations.soa_members.name[member])};
                expression += logical_column_access(context, schema, path) + "[index]";
            }
            expression += constructor ? ")" : "}";
        }
        header
            .add(ir.header_function([&] {
                auto const created_function{ir.function("operator[]", "auto")};
                ir.set_function_parameters(created_function,
                                           {ir.parameter("std::uint32_t", "index")});
                ir.set_function_body(
                    created_function,
                    {ir.raw("assert(index < num());\nreturn " + expression + ";")});
                ir.set_function_return_type(
                    created_function, ir.types.intern("equivalent_type"), ReturnSyntax::trailing);
                ir.set_function_receiver(created_function, FunctionReceiver::const_);
                ir.function_flags(created_function).set_is_inline(true);
                ir.function_flags(created_function).set_define_in_header(true);
                return created_function;
            }()))
            .new_lines(1);
    }
    return {header.build(), source.build()};
}
}
