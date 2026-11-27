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
                                        {dependency(operations,
                                                    receiver == SoaReceiver::owner
                                                        ? "ioj/single_allocation/operations.h"
                                                        : "ioj/single_allocation/view.h")}),
                             {named_type(type_name), base});
    }
    auto lower_soa_api(LoweringContext& parent_context,
                       DeclarationHandle schema,
                       SoaReceiver const receiver,
                       std::string const& type_name) -> LoweredSoa {
        auto ctx{parent_context.for_declaration(schema)};
        auto const& soas{ctx.declarations.soas};
        auto header{node_list()};
        auto source{node_list()};
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
                std::vector<TypeDependencyId> dependencies;
                for (auto const key : ctx.declarations.items(function_schemas.dependencies[row])) {
                    append_type_dependencies(dependencies, ctx.text(key), ctx.types);
                }
                Nodes implementation{raw_parts(body, dependencies)};
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
                    append_body.add(variable("auto const", index_name, this_member_call("num")))
                        .add(expr_stmt(this_member_call("add_defaulted", {literal(1)})));
                    auto const count{columns.size()};
                    for (std::size_t column{}; column < count; ++column) {
                        append_body.add(
                            assignment(subscript(member_call(this_member_call("view"),
                                                             ctx.text(columns[column])),
                                                 index),
                                       named(parameter_schemas.name[parameters[column]])));
                    }
                    auto const return_type{
                        ctx.resolve(function_schemas.trailing_return_type[row].value_or(
                            function_schemas.return_type[row]))};
                    auto const spelling{type_spelling(return_type)};
                    append_body.add(return_stmt(spelling == "auto" || spelling == "decltype(auto)"
                                                    ? index
                                                    : init_list({index}, return_type)));
                    implementation = append_body.build();
                }
                auto const spec{
                    lower_schema_function(ctx, function_id, std::move(implementation), true)};
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
        return {header.build(), source.build()};
    }
    auto schema_rows(LoweringContext& ctx, DeclarationHandle schema) -> Nodes {
        auto const& soas{ctx.declarations.soas};
        auto const& members{ctx.declarations.soa_members};
        auto result{node_list()};
        if (soas.equivalent_type[schema]) {
            result
                .add(using_declaration(store_text("equivalent_type"),
                                       ctx.resolve(*soas.equivalent_type[schema])))
                .new_lines(1);
            std::vector<Syntax> args;
            for (auto member : ctx.declarations.items(soas.members[schema])) {
                auto name{ctx.text(members.name[member])};
                if (members.kind[member] == SoaMemberKind::nested) {
                    name = "view_" + name;
                }
                args.push_back(subscript(member_call(named("columns"), name), named("index")));
            }
            auto const constructor{soas.equivalent_constructor[schema]};
            auto const value{constructor
                                 ? call(reference_type(intern_type(ctx.text(*constructor))), args)
                                 : init_list(args, named_type("equivalent_type"))};
            FunctionDefinition const function{
                .name = this->name("operator[]"),
                .return_type = named_type("equivalent_type"),
                .return_syntax = ReturnSyntax::trailing,
                .parameters = {parameter("this auto const&", "self"),
                               parameter(std_types().u32, "index")},
                .implementation = {.kind = FunctionImplementationKind::body,
                                   .body = {assertion(binary(BinaryOperator::less,
                                                             named("index"),
                                                             member_call(named("self"), "num"))),
                                            variable("auto const&",
                                                     "columns",
                                                     member_call(named("self"), "view")),
                                            return_stmt(value)}},
                .flags = {.define_in_header = true}};
            result.add(header_function(add_function(function))).new_lines(1);
        }

        auto mapped = [&](auto&& self, DeclarationHandle current) -> bool {
            for (auto member : ctx.declarations.items(soas.members[current])) {
                if (!members.equivalent_member[member] ||
                    (members.kind[member] == SoaMemberKind::nested &&
                     !self(self, ctx.for_declaration(current).nested_declaration(member)))) {
                    return false;
                }
            }
            return soas.equivalent_type[current].has_value();
        };
        if (mapped(mapped, schema)) {
            result
                .add(using_declaration(store_text("writable_equivalent"),
                                       named_type("equivalent_type")))
                .new_lines(1);
            std::vector<Syntax> values;
            auto flatten = [&](auto&& self, DeclarationHandle current, Expr parent) -> void {
                for (auto member : ctx.declarations.items(soas.members[current])) {
                    auto value{member_access(parent, *members.equivalent_member[member])};
                    if (members.equivalent_accessor[member]) {
                        value = call(value);
                    }
                    if (members.kind[member] == SoaMemberKind::nested) {
                        self(self, ctx.for_declaration(current).nested_declaration(member), value);
                    } else {
                        values.push_back(value);
                    }
                }
            };
            flatten(flatten, schema, named("value"));
            FunctionDefinition const function{
                .name = this->name("equivalent_values"),
                .return_type = intern_type("decltype(auto)"),
                .parameters = {parameter(
                                   decorate_type(named_type("equivalent_type"), "", " const&"),
                                   "value"),
                               parameter("auto&&", "func")},
                .implementation = {.kind = FunctionImplementationKind::body,
                                   .body = {return_stmt(call(this->name("func"), values))}},
                .receiver = FunctionReceiver::static_,
                .flags = {.define_in_header = true}};
            result.add(header_function(add_function(function))).new_lines(1);
        }
        return result.build();
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
    auto expand_columns(LoweringContext& ctx,
                        std::string_view const body,
                        DeclarationHandle schema,
                        SoaReceiver const receiver) -> std::vector<Syntax> {
        std::vector<Syntax> result;
        std::size_t copied{};
        scan_soa_columns(
            body,
            ctx.declarations.strings().view(ctx.declarations.soas.name[schema]),
            [&](std::size_t const begin,
                std::size_t const end,
                std::span<std::string const> const path) {
                result.push_back(authored_fragment(body.substr(copied, begin - copied)));
                auto const owner{receiver == SoaReceiver::owner};
                auto const visibility{logical_column_access(
                    ctx, schema, path, owner ? call(this->name("view")) : named("this"), !owner)};
                result.push_back(visibility);
                copied = end;
            });
        result.push_back(authored_fragment(body.substr(copied)));
        return result;
    }
};

} // namespace

auto schema_rows(LoweringContext& ctx, DeclarationHandle schema) -> Nodes {
    return SoaApiLowering{ctx.ir}.schema_rows(ctx, schema);
}
auto soa_row_operations(LoweringContext& ctx,
                        DeclarationHandle schema,
                        SoaReceiver receiver,
                        std::string const& type_name,
                        CppTypeId base) -> CppTypeId {
    return SoaApiLowering{ctx.ir}.soa_row_operations(ctx, schema, receiver, type_name, base);
}
auto lower_soa_api(LoweringContext& ctx,
                   DeclarationHandle schema,
                   SoaReceiver const receiver,
                   std::string const& type_name) -> LoweredSoa {
    return SoaApiLowering{ctx.ir}.lower_soa_api(ctx, schema, receiver, type_name);
}

} // namespace lispb::codegen::detail
