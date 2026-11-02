#include "soa_api.h"

#include "lowered_soa.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "single_allocation_soa_model.h"
#include "soa_body.h"
#include "soa_receiver.h"

#include <codegen/ast/cpp_ir_builder.h>
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
namespace {

class SoaApiLowering : public CppIRBuilder {
  public:
    explicit SoaApiLowering(CppIR& ir)
        : CppIRBuilder{ir} {}

    /* **************************************** */
    // Lowering
    /* **************************************** */
    auto logical_column_access(LoweringContext& ctx,
                               DeclarationHandle schema,
                               std::span<std::string const> const path,
                               Expr receiver = {},
                               bool pointer = false) -> Expr {
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
            auto const accessor{!receiver ? named(name)
                                : pointer ? pointer_member_access(receiver, name)
                                          : member_access(receiver, name)};
            receiver = call(accessor);
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
    auto soa_row_operations(LoweringContext& ctx,
                            DeclarationHandle schema,
                            SoaReceiver receiver,
                            std::string const& type_name,
                            CppTypeId base) -> CppTypeId {
        if (receiver == SoaReceiver::const_view ||
            has_custom_row_function(ctx, schema, receiver, "set")) {
            return base;
        }
        auto const operations{receiver == SoaReceiver::owner ? "OwnerRowOperations"
                                                             : "ViewRowOperations"};
        return template_type(named_type("ioj::soa_storage_detail::" + std::string{operations},
                                        NameOrigin::global,
                                        {dependency(operations, "ioj/single_allocation/view.h")}),
                             {named_type(type_name),
                              named_type(schema_namespace(ctx, schema) + "::Layout::column_types",
                                         NameOrigin::global),
                              base});
    }
    auto lower_soa_api(LoweringContext& parent_context,
                       DeclarationHandle schema,
                       SoaReceiver const receiver,
                       std::string const& type_name,
                       CppTypeId base) -> LoweredSoa {
        auto ctx{parent_context.for_declaration(schema)};
        auto const& soas{ctx.declarations.soas};
        auto header{node_list()};
        auto source{node_list()};
        header.append(compact_row_api(ctx, schema, receiver, base));
        for (auto const alias : ctx.declarations.items(soas.using_declarations[schema])) {
            header.add(raw("using " + ctx.text(alias) + ";")).new_lines(1);
        }
        auto append = [&](SchemaRange<FunctionSchemaId> const functions) {
            for (auto const function_id : ctx.declarations.items(functions)) {
                auto const& function_schemas{ctx.declarations.functions};
                auto const row{function_schemas.index(function_id)};
                auto const body{
                    expand_columns(ctx,
                                   join_lines(ctx, function_schemas.body_lines[function_id]),
                                   schema,
                                   receiver)};
                auto spec{lower_schema_function(ctx, function_id, body)};
                if (auto const appended{function_schemas.append_columns[row]}) {
                    auto const columns{ctx.declarations.items(*appended)};
                    auto const parameters{ctx.declarations.items(function_schemas.parameters[row])};
                    auto const& parameter_schemas{ctx.declarations.parameters};
                    std::string index_name{"row"};
                    while (std::ranges::any_of(parameters, [&](auto parameter_id) {
                        return ctx.text(parameter_schemas.name[parameter_id]) == index_name;
                    })) {
                        index_name += '_';
                    }
                    auto const index{named(index_name)};
                    auto append_body{node_list()};
                    append_body.add(variable("auto const", index_name, call(this_member("num"))))
                        .add(expr_stmt(call(this_member("add_defaulted"), {literal("1")})));
                    auto const count{columns.size()};
                    for (std::size_t column{}; column < count; ++column) {
                        append_body.add(
                            assignment(subscript(call(member_access(call(this_member("view")),
                                                                    ctx.text(columns[column]))),
                                                 index),
                                       named(parameter_schemas.name[parameters[column]])));
                    }
                    auto const return_type{spec.return_type};
                    auto const spelling{type_spelling(return_type)};
                    append_body.add(return_stmt(spelling == "auto" || spelling == "decltype(auto)"
                                                    ? index
                                                    : init_list({index}, return_type)));
                    spec.implementation = {.kind = FunctionImplementationKind::body,
                                           .body = append_body.build()};
                    spec.flags.set_define_in_header(!function_schemas.definition_in_source[row]);
                }
                if (function_schemas.definition_in_source[function_id]) {
                    auto const return_syntax{spec.return_syntax};
                    auto const return_spelling{type_spelling(spec.return_type)};
                    auto const use_trailing_return{return_syntax == ReturnSyntax::direct &&
                                                   return_spelling != "auto" &&
                                                   return_spelling != "decltype(auto)"};
                    auto const published{add_function(spec)};
                    header.add(declaration(published)).new_lines(1);
                    source
                        .add(definition(published,
                                        intern_type(type_name),
                                        use_trailing_return ? ReturnSyntax::trailing
                                                            : return_syntax))
                        .new_lines(2);
                } else {
                    spec.flags.set_define_in_header(
                        spec.flags.define_in_header() ||
                        !function_schemas.body_lines[function_id].empty());
                    spec.flags.set_is_inline(spec.flags.define_in_header());
                    header.add(header_function(add_function(spec))).new_lines(1);
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
            header.add(using_declaration(store_text("equivalent_type"), equivalent)).new_lines(1);
            Expr expression;
            if (receiver == SoaReceiver::owner) {
                expression = subscript(call(named("const_view")), named("index"));
            } else {
                auto const constructor_id{soas.equivalent_constructor[schema]};
                std::vector<Expr> arguments;
                for (auto const member : ctx.declarations.items(soas.members[schema])) {
                    std::vector<std::string> const path{
                        ctx.text(ctx.declarations.soa_members.name[member])};
                    arguments.push_back(
                        subscript(logical_column_access(ctx, schema, path), named("index")));
                }
                expression = constructor_id ? call(type_reference(reference_type(
                                                       intern_type(ctx.text(*constructor_id)))),
                                                   arguments)
                                            : init_list(arguments, named_type("equivalent_type"));
            }
            FunctionDefinition function_id{.name = named("operator[]"),
                                           .return_type = intern_type("auto")};
            function_id.parameters = {parameter(std_types().u32, "index")};
            function_id.implementation = {
                .kind = FunctionImplementationKind::body,
                .body = {
                    assertion(binary(BinaryOperator::less, named("index"), call(named("num")))),
                    return_stmt(expression)}};
            function_id.return_type = intern_type("equivalent_type");
            function_id.return_syntax = ReturnSyntax::trailing;
            function_id.receiver = FunctionReceiver::const_;
            auto& flags{function_id.flags};
            flags.set_is_inline(true);
            flags.set_define_in_header(true);
            header.add(header_function(add_function(function_id))).new_lines(1);
        }
        return {header.build(), source.build()};
    }
  private:
    /* **************************************** */
    // Construction helpers
    /* **************************************** */
    auto has_custom_row_function(LoweringContext& ctx,
                                 DeclarationHandle schema,
                                 SoaReceiver receiver,
                                 std::string_view name) -> bool {
        auto const& decls{ctx.declarations};
        auto const& soas{decls.soas};
        auto const row{soas.index(schema)};
        auto const functions{receiver == SoaReceiver::owner ? soas.functions[row]
                                                            : soas.mutable_view_functions[row]};
        return std::ranges::any_of(decls.items(functions), [&](auto function_id) {
            return ctx.text(decls.functions.name[function_id]) == name;
        });
    }
    auto compact_row_api(LoweringContext& ctx,
                         DeclarationHandle schema,
                         SoaReceiver receiver,
                         CppTypeId base) -> Nodes {
        auto const& decls{ctx.declarations};
        auto const& soas{decls.soas};
        auto const& members{decls.soa_members};
        auto result{node_list()};
        auto const owner{receiver == SoaReceiver::owner};
        auto const mutable_view{receiver == SoaReceiver::mutable_view};
        auto custom = [&](std::string_view name) {
            return has_custom_row_function(ctx, schema, receiver, name);
        };
        auto emit = [&](std::string_view name,
                        std::vector<FunctionParameterId> parameters,
                        Nodes body) {
            FunctionDefinition function_id{.name = named(name),
                                           .return_type = name == "set" ? std_types().void_
                                                                        : named_type("size_type")};
            if (name == "set") {
                parameters.insert(parameters.begin(), parameter("size_type", "index"));
                function_id.receiver =
                    mutable_view ? FunctionReceiver::const_ : FunctionReceiver::unqualified;
            }
            function_id.parameters.assign(parameters.begin(), parameters.end());
            function_id.implementation = {.kind = FunctionImplementationKind::body, .body = body};
            function_id.flags.set_define_in_header(true);
            function_id.flags.set_is_inline(true);
            result.add(header_function(add_function(function_id))).new_lines(1);
        };

        if (owner) {
            for (auto member : decls.items(soas.members[schema])) {
                auto const row{members.index(member)};
                auto const name{(members.kind[row] == SoaMemberKind::nested ? "view_" : "") +
                                ctx.text(members.name[row])};
                FunctionDefinition function_id{.name = named(name),
                                               .return_type = intern_type("auto")};
                function_id.parameters = {
                    parameter(decorate_type(intern_type("auto"), "this ", "&"), "self")};
                function_id.implementation = {
                    .kind = FunctionImplementationKind::body,
                    .body = {return_stmt(
                        call(member_access(call(member_access(named("self"), "view")), name)))}};
                function_id.flags.set_define_in_header(true);
                function_id.formatting.body_layout = FunctionBodyLayout::compact;
                result.add(header_function(add_function(function_id))).new_lines(1);
            }
        }
        if ((!owner && !mutable_view) || custom("set")) {
            return result.build();
        }
        if (owner) {
            result.add(using_declaration(store_text("RowOperations"), base)).new_lines(1);
        }
        result.add(using_import(named_type(owner ? "RowOperations::set" : "Base::set")))
            .new_lines(1);
        if (owner && !custom("add")) {
            result.add(using_import(named_type("RowOperations::add"))).new_lines(1);
        }

        std::vector<CppTypeId> parameter_types;
        std::vector<Expr> equivalent_arguments;
        auto mapped{soas.equivalent_type[schema].has_value()};
        auto collect = [&](auto&& self, DeclarationHandle current, Expr value) -> void {
            for (auto member : decls.items(soas.members[current])) {
                auto const row{members.index(member)};
                auto component{value};
                if (members.equivalent_member[row]) {
                    component = member_access(component, *members.equivalent_member[row]);
                    if (members.equivalent_accessor[row]) {
                        component = call(component);
                    }
                } else {
                    mapped = false;
                }
                if (members.kind[row] == SoaMemberKind::nested) {
                    self(self, ctx.for_declaration(current).nested_declaration(member), component);
                    continue;
                }
                auto const type{ctx.for_declaration(current).resolve(members.type[row])};
                parameter_types.push_back(decorate_type(type, "", " const"));
                equivalent_arguments.push_back(component);
            }
        };
        collect(collect, schema, named("value"));
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
        std::vector<Expr> logical_arguments{named("index")};
        auto logical_assignments{node_list()};
        logical_assignments.add(
            assertion(binary(BinaryOperator::less, named("index"), call(named("num")))));
        auto collect_logical = [&](auto&& self,
                                   DeclarationHandle current,
                                   std::vector<std::string> const& path) -> void {
            for (auto const member : decls.items(soas.members[current])) {
                auto const row{members.index(member)};
                auto column{path};
                column.push_back(ctx.text(members.name[row]));
                auto member_context{ctx.for_declaration(current)};
                auto type_reference_value{members.type[row]};
                bool const nested{members.kind[row] == SoaMemberKind::nested};
                if (nested) {
                    auto const child{member_context.nested_declaration(member)};
                    if (!can_assign_equivalent(can_assign_equivalent, child)) {
                        self(self, child, column);
                        continue;
                    }
                    member_context.module = ctx.for_declaration(child).module;
                    type_reference_value = *soas.equivalent_type[child];
                }
                auto const name{"new_" + join(column, "_")};
                auto const type{member_context.resolve(type_reference_value)};
                logical_parameters.push_back(parameter(decorate_type(type, "", " const"), name));
                logical_arguments.push_back(named(name));
                auto const visibility{logical_column_access(
                    ctx, schema, column, owner ? call(named("view")) : named("this"), !owner)};
                logical_assignments.add(
                    nested ? expr_stmt(call(member_access(visibility, "set"),
                                            {named("index"), named(name)}))
                           : assignment(subscript(visibility, named("index")), named(name)));
            }
        };
        collect_logical(collect_logical, schema, {});
        auto const same_parameters{std::ranges::equal(
            parameter_types,
            logical_parameters,
            {},
            [&](auto type) { return type_spelling(type); },
            [&](auto parameter_id) { return type_spelling(get_view(parameter_id).type); })};
        if (!same_parameters) {
            emit("set", logical_parameters, logical_assignments.build());
            if (owner && !custom("add")) {
                emit("add",
                     logical_parameters,
                     {variable("auto const", "index", call(named("num"))),
                      expr_stmt(call(named("add_uninitialised"), {literal("1")})),
                      expr_stmt(call(named("set"), logical_arguments)),
                      return_stmt(named("index"))});
            }
        }
        if (mapped) {
            auto const equivalent{ctx.resolve(*soas.equivalent_type[schema])};
            auto const value{parameter(decorate_type(equivalent, "", " const"), "value")};
            auto set_arguments{equivalent_arguments};
            set_arguments.insert(set_arguments.begin(), named("index"));
            emit("set", {value}, {expr_stmt(call(named("set"), set_arguments))});
            if (owner && !custom("add")) {
                emit("add", {value}, {return_stmt(call(named("add"), equivalent_arguments))});
            }
        }
        return result.build();
    }
    auto expand_columns(LoweringContext& ctx,
                        std::string_view const body,
                        DeclarationHandle schema,
                        SoaReceiver const receiver) -> std::vector<Expr> {
        std::vector<Expr> result;
        std::size_t copied{};
        scan_soa_columns(
            body,
            ctx.declarations.strings().view(ctx.declarations.soas.name[schema]),
            [&](std::size_t const begin,
                std::size_t const end,
                std::span<std::string const> const path) {
                result.push_back(raw_expr(body.substr(copied, begin - copied)));
                auto const owner{receiver == SoaReceiver::owner};
                auto const visibility{logical_column_access(
                    ctx, schema, path, owner ? call(named("view")) : named("this"), !owner)};
                result.push_back(visibility);
                copied = end;
            });
        result.push_back(raw_expr(body.substr(copied)));
        return result;
    }
};

} // namespace

auto logical_column_access(LoweringContext& ctx,
                           DeclarationHandle schema,
                           std::span<std::string const> const path,
                           Expr receiver = {},
                           bool pointer = false) -> Expr {
    return SoaApiLowering{ctx.ir}.logical_column_access(ctx, schema, path, receiver, pointer);
}
auto soa_row_operations(LoweringContext& ctx,
                        DeclarationHandle schema,
                        SoaReceiver receiver,
                        std::string const& type_name,
                        CppTypeId base) -> CppTypeId {
    return SoaApiLowering{ctx.ir}.soa_row_operations(ctx, schema, receiver, type_name, base);
}
auto lower_soa_api(LoweringContext& parent_context,
                   DeclarationHandle schema,
                   SoaReceiver const receiver,
                   std::string const& type_name,
                   CppTypeId base) -> LoweredSoa {
    return SoaApiLowering{parent_context.ir}.lower_soa_api(
        parent_context, schema, receiver, type_name, base);
}

} // namespace lispb::codegen::detail
