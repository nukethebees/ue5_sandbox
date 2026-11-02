#include "soa_api.h"

#include "lowered_soa.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_model.h"
#include "soa_body.h"
#include "soa_receiver.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_body_layout.h>
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
auto logical_column_access(LoweringContext& ctx,
                           DeclarationHandle schema,
                           std::span<std::string const> const path,
                           Expr receiver = {},
                           bool pointer = false) -> Expr {
    auto& ir{ctx.ir};
    if (path.empty()) {
        throw std::invalid_argument{"SOA column reference cannot be empty"};
    }
    auto const& soas{ctx.declarations.soas};
    auto const& columns{ctx.declarations.soa_members};
    auto current{schema};
    for (std::size_t index{}; index < path.size(); ++index) {
        auto const members{ctx.declarations.items(soas.members[current])};
        auto const found{std::ranges::find(members, path[index], [&](auto id) {
            return ctx.declarations.strings().view(columns.name[id]);
        })};
        if (found == members.end()) {
            throw std::invalid_argument{"SOA '" + ctx.text(soas.name[current]) +
                                        "' has no column '" + path[index] + "'"};
        }
        auto const member{*found};
        bool const nested{columns.kind[member] == SoaMemberKind::nested};
        auto const name{(nested ? "view_" : "") + path[index]};
        auto const accessor{!receiver ? ir.named(name)
                            : pointer ? ir.pointer_member_access(receiver, name)
                                      : ir.member_access(receiver, name)};
        receiver = ir.call(accessor);
        pointer = false;
        if (index + 1 < path.size()) {
            if (!nested) {
                throw std::invalid_argument{"SOA column path requires a nested column: " +
                                            path[index]};
            }
            current = ctx.for_declaration(current).nested_declaration(member);
        }
    }
    return receiver;
}

namespace {
auto has_custom_row_function(LoweringContext& ctx,
                             DeclarationHandle schema,
                             SoaReceiver receiver,
                             std::string_view name) -> bool {
    auto const& decls{ctx.declarations};
    auto const& soas{decls.soas};
    auto const row{soas.index(schema)};
    auto const functions{receiver == SoaReceiver::owner ? soas.functions[row]
                                                        : soas.mutable_view_functions[row]};
    return std::ranges::any_of(decls.items(functions), [&](auto function) {
        return ctx.text(decls.functions.name[function]) == name;
    });
}

auto compact_row_api(LoweringContext& ctx,
                     DeclarationHandle schema,
                     SoaReceiver receiver,
                     CppTypeId base) -> Nodes {
    auto& ir{ctx.ir};
    auto const& decls{ctx.declarations};
    auto const& soas{decls.soas};
    auto const& members{decls.soa_members};
    NodeListBuilder result{ir};
    auto const owner{receiver == SoaReceiver::owner};
    auto const mutable_view{receiver == SoaReceiver::mutable_view};
    auto custom = [&](std::string_view name) {
        return has_custom_row_function(ctx, schema, receiver, name);
    };
    auto emit = [&](std::string_view name,
                    std::vector<FunctionParameterId> parameters,
                    Nodes body) {
        auto const function{
            ir.function(name, name == "set" ? ir.types.std.void_ : ir.types.named("size_type"))};
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
        for (auto member : decls.items(soas.members[schema])) {
            auto const row{members.index(member)};
            auto const name{(members.kind[row] == SoaMemberKind::nested ? "view_" : "") +
                            ctx.text(members.name[row])};
            auto const function{ir.function(name, "auto")};
            ir.set_function_parameters(
                function,
                {ir.parameter(ir.types.decorate(ir.types.intern("auto"), "this ", "&"), "self")});
            ir.set_function_body(function,
                                 {ir.return_stmt(ir.call(ir.member_access(
                                     ir.call(ir.member_access(ir.named("self"), "view")), name)))});
            ir.function_flags(function).set_define_in_header(true);
            ir.function_formatting(function).body_layout = FunctionBodyLayout::compact;
            result.add(ir.header_function(function)).new_lines(1);
        }
    }
    if ((!owner && !mutable_view) || custom("set")) {
        return result.build();
    }
    if (owner) {
        result.add(ir.using_declaration(ir.strings.intern("RowOperations"), base)).new_lines(1);
    }
    result.add(ir.using_import(ir.types.named(owner ? "RowOperations::set" : "Base::set")))
        .new_lines(1);
    if (owner && !custom("add")) {
        result.add(ir.using_import(ir.types.named("RowOperations::add"))).new_lines(1);
    }

    std::vector<CppTypeId> parameter_types;
    std::vector<Expr> equivalent_arguments;
    auto mapped{soas.equivalent_type[schema].has_value()};
    auto collect = [&](auto&& self, DeclarationHandle current, Expr value) -> void {
        for (auto member : decls.items(soas.members[current])) {
            auto const row{members.index(member)};
            auto component{value};
            if (members.equivalent_member[row]) {
                component = ir.member_access(component, *members.equivalent_member[row]);
                if (members.equivalent_accessor[row]) {
                    component = ir.call(component);
                }
            } else {
                mapped = false;
            }
            if (members.kind[row] == SoaMemberKind::nested) {
                self(self, ctx.for_declaration(current).nested_declaration(member), component);
                continue;
            }
            auto const type{ctx.for_declaration(current).resolve(members.type[row])};
            parameter_types.push_back(ir.types.decorate(type, "", " const"));
            equivalent_arguments.push_back(component);
        }
    };
    collect(collect, schema, ir.named("value"));
    auto can_assign_equivalent = [&](auto&& self, DeclarationHandle current) -> bool {
        if (!soas.equivalent_type[current]) {
            return false;
        }
        for (auto const member : decls.items(soas.members[current])) {
            auto const row{members.index(member)};
            if (!members.equivalent_member[row]) {
                return false;
            }
            if (members.kind[row] == SoaMemberKind::nested &&
                !self(self, ctx.for_declaration(current).nested_declaration(member))) {
                return false;
            }
        }
        return true;
    };
    std::vector<FunctionParameterId> logical_parameters;
    std::vector<Expr> logical_arguments{ir.named("index")};
    NodeListBuilder logical_assignments{ir};
    logical_assignments.add(ir.expr_stmt(
        ir.call(ir.named("assert"),
                {ir.binary(BinaryOperator::less, ir.named("index"), ir.call(ir.named("num")))})));
    auto collect_logical =
        [&](auto&& self, DeclarationHandle current, std::vector<std::string> const& path) -> void {
        for (auto const member : decls.items(soas.members[current])) {
            auto const row{members.index(member)};
            auto column{path};
            column.push_back(ctx.text(members.name[row]));
            auto member_context{ctx.for_declaration(current)};
            auto type_reference{members.type[row]};
            bool const nested{members.kind[row] == SoaMemberKind::nested};
            if (nested) {
                auto const child{member_context.nested_declaration(member)};
                if (!can_assign_equivalent(can_assign_equivalent, child)) {
                    self(self, child, column);
                    continue;
                }
                member_context.module = ctx.for_declaration(child).module;
                type_reference = *soas.equivalent_type[child];
            }
            auto const name{"new_" + join(column, "_")};
            auto const type{member_context.resolve(type_reference)};
            logical_parameters.push_back(ir.parameter(ir.types.decorate(type, "", " const"), name));
            logical_arguments.push_back(ir.named(name));
            auto const access{logical_column_access(
                ctx, schema, column, owner ? ir.call(ir.named("view")) : ir.named("this"), !owner)};
            logical_assignments.add(
                nested ? ir.expr_stmt(ir.call(ir.member_access(access, "set"),
                                              {ir.named("index"), ir.named(name)}))
                       : ir.assignment(ir.subscript(access, ir.named("index")), ir.named(name)));
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
                 {ir.variable("auto const", "index", ir.call(ir.named("num"))),
                  ir.expr_stmt(ir.call(ir.named("add_uninitialised"), {ir.literal("1")})),
                  ir.expr_stmt(ir.call(ir.named("set"), logical_arguments)),
                  ir.return_stmt(ir.named("index"))});
        }
    }
    if (mapped) {
        auto const equivalent{ctx.resolve(*soas.equivalent_type[schema])};
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

auto expand_columns(LoweringContext& ctx,
                    std::string_view const body,
                    DeclarationHandle schema,
                    SoaReceiver const receiver) -> std::vector<Expr> {
    auto& ir{ctx.ir};
    std::vector<Expr> result;
    std::size_t copied{};
    scan_soa_columns(
        body,
        ctx.declarations.strings().view(ctx.declarations.soas.name[schema]),
        [&](std::size_t const begin,
            std::size_t const end,
            std::span<std::string const> const path) {
            result.push_back(ir.raw_expr(body.substr(copied, begin - copied)));
            auto const owner{receiver == SoaReceiver::owner};
            auto const access{logical_column_access(
                ctx, schema, path, owner ? ir.call(ir.named("view")) : ir.named("this"), !owner)};
            result.push_back(access);
            copied = end;
        });
    result.push_back(ir.raw_expr(body.substr(copied)));
    return result;
}
}

auto soa_row_operations(LoweringContext& ctx,
                        DeclarationHandle schema,
                        SoaReceiver receiver,
                        std::string const& type_name,
                        CppTypeId base) -> CppTypeId {
    if (receiver == SoaReceiver::const_view ||
        has_custom_row_function(ctx, schema, receiver, "set")) {
        return base;
    }
    auto& types{ctx.ir.types};
    auto const operations{receiver == SoaReceiver::owner ? "OwnerRowOperations"
                                                         : "ViewRowOperations"};
    return types.template_type(
        types.named("ioj::soa_storage_detail::" + std::string{operations},
                    NameOrigin::global,
                    {types.dependency(operations, "ioj/single_allocation/view.h")}),
        {types.named(type_name),
         types.named(schema_namespace(ctx, schema) + "::Layout::column_types", NameOrigin::global),
         base});
}

auto lower_soa_api(LoweringContext& parent_context,
                   DeclarationHandle schema,
                   SoaReceiver const receiver,
                   std::string const& type_name,
                   CppTypeId base) -> LoweredSoa {
    auto ctx{parent_context.for_declaration(schema)};
    auto& ir{ctx.ir};
    auto const& soas{ctx.declarations.soas};
    NodeListBuilder header{ir};
    NodeListBuilder source{ir};
    header.append(compact_row_api(ctx, schema, receiver, base));
    for (auto const alias : ctx.declarations.items(soas.using_declarations[schema])) {
        header.add(ir.raw("using " + ctx.text(alias) + ";")).new_lines(1);
    }
    auto append = [&](SchemaRange<FunctionSchemaId> const functions) {
        for (auto const function : ctx.declarations.items(functions)) {
            auto const& function_schemas{ctx.declarations.functions};
            auto const row{function_schemas.index(function)};
            auto const body{expand_columns(
                ctx, join_lines(ctx, function_schemas.body_lines[function]), schema, receiver)};
            auto const spec{lower_schema_function(ctx, function, body)};
            if (auto const appended{function_schemas.append_columns[row]}) {
                auto const columns{ctx.declarations.items(*appended)};
                auto const parameters{ctx.declarations.items(function_schemas.parameters[row])};
                auto const& parameter_schemas{ctx.declarations.parameters};
                std::string index_name{"row"};
                while (std::ranges::any_of(parameters, [&](auto parameter) {
                    return ctx.text(parameter_schemas.name[parameter]) == index_name;
                })) {
                    index_name += '_';
                }
                auto const index{ir.named(index_name)};
                NodeListBuilder append_body{ir};
                append_body
                    .add(ir.variable("auto const", index_name, ir.call(ir.this_member("num"))))
                    .add(ir.expr_stmt(ir.call(ir.this_member("add_defaulted"), {ir.literal("1")})));
                auto const count{columns.size()};
                for (std::size_t column{}; column < count; ++column) {
                    append_body.add(ir.assignment(
                        ir.subscript(ir.call(ir.member_access(ir.call(ir.this_member("view")),
                                                              ctx.text(columns[column]))),
                                     index),
                        ir.named(parameter_schemas.name[parameters[column]])));
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
            if (function_schemas.definition_in_source[function]) {
                auto const return_syntax{ir.function_return_syntax(spec)};
                auto const return_spelling{ir.types.spelling(ir.function_return_type(spec))};
                auto const use_trailing_return{return_syntax == ReturnSyntax::direct &&
                                               return_spelling != "auto" &&
                                               return_spelling != "decltype(auto)"};
                header.add(ir.declaration(spec)).new_lines(1);
                source
                    .add(
                        ir.definition(spec,
                                      ir.types.intern(type_name),
                                      use_trailing_return ? ReturnSyntax::trailing : return_syntax))
                    .new_lines(2);
            } else {
                ir.function_flags(spec).set_define_in_header(
                    ir.function_flags(spec).define_in_header() ||
                    !function_schemas.body_lines[function].empty());
                ir.function_flags(spec).set_is_inline(ir.function_flags(spec).define_in_header());
                header.add(ir.header_function(spec)).new_lines(1);
            }
        }
    };
    if (receiver == SoaReceiver::owner) {
        append(soas.functions[schema]);
    } else {
        append(soas.const_view_functions[schema]);
        if (receiver == SoaReceiver::mutable_view) {
            append(soas.mutable_view_functions[schema]);
        }
    }
    if (soas.equivalent_type[schema]) {
        auto const equivalent{ctx.resolve(*soas.equivalent_type[schema])};
        header.add(ir.using_declaration(ir.strings.intern("equivalent_type"), equivalent))
            .new_lines(1);
        Expr expression;
        if (receiver == SoaReceiver::owner) {
            expression = ir.subscript(ir.call(ir.named("const_view")), ir.named("index"));
        } else {
            auto const constructor{soas.equivalent_constructor[schema]};
            std::vector<Expr> arguments;
            for (auto const member : ctx.declarations.items(soas.members[schema])) {
                std::vector<std::string> const path{
                    ctx.text(ctx.declarations.soa_members.name[member])};
                arguments.push_back(
                    ir.subscript(logical_column_access(ctx, schema, path), ir.named("index")));
            }
            expression = constructor ? ir.call(ir.type_reference(ir.types.reference(
                                                   ir.types.intern(ctx.text(*constructor)))),
                                               arguments)
                                     : ir.init_list(arguments, ir.types.named("equivalent_type"));
        }
        auto const function{ir.function("operator[]", "auto")};
        ir.set_function_parameters(function, {ir.parameter(ir.types.std.u32, "index")});
        ir.set_function_body(
            function,
            {ir.expr_stmt(ir.call(
                 ir.named("assert"),
                 {ir.binary(BinaryOperator::less, ir.named("index"), ir.call(ir.named("num")))})),
             ir.return_stmt(expression)});
        ir.set_function_return_type(
            function, ir.types.intern("equivalent_type"), ReturnSyntax::trailing);
        ir.set_function_receiver(function, FunctionReceiver::const_);
        auto& flags{ir.function_flags(function)};
        flags.set_is_inline(true);
        flags.set_define_in_header(true);
        header.add(ir.header_function(function)).new_lines(1);
    }
    return {header.build(), source.build()};
}
}
