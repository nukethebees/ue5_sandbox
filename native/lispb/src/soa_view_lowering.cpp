#include "lowered_soa.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_functions.h"
#include "soa_internal.h"

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/soa_member_kind.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cstddef>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto view_expression(LoweringContext& context, ResolvedMemberId member, bool const_view) -> Expr {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto const row{resolved_store.index(member)};
    auto& ir{context.ir};
    auto const column{ir.named(resolved_store.name[row])};
    if (resolved_store.kind[row] == SoaMemberKind::nested) {
        return ir.call(ir.member_access(column, const_view ? "get_const_view" : "get_view"),
                       {ir.named("offset"), ir.named("count")});
    }

    auto const type{const_view ? resolved_store.const_view_type[row]
                               : resolved_store.view_type[row]};
    return ir.call(ir.member_access(ir.init_list({column}, type), "Slice"),
                   {ir.named("offset"), ir.named("count")});
}

auto equivalent_expression(LoweringContext& context, ResolvedMemberId member, Expr index) -> Expr {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto const row{resolved_store.index(member)};
    auto& ir{context.ir};
    auto const column{ir.named(resolved_store.name[row])};
    auto const values{resolved_store.kind[row] == SoaMemberKind::array
                          ? ir.call(ir.member_access(column, "GetData"))
                          : column};
    return ir.subscript(values, index);
}

auto column_names(LoweringContext& context, std::span<ResolvedMemberId const> members)
    -> std::vector<std::string> {
    auto& resolved_store{context.ir.lowering.resolved_members};
    std::vector<std::string> result;
    result.reserve(members.size());
    for (auto const member : members) {
        result.push_back(context.text(resolved_store.name[resolved_store.index(member)]));
    }
    return result;
}

auto declaration_nodes(LoweringContext& context, std::span<FunctionId const> const specs) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    auto const count{specs.size()};
    for (std::size_t index{0}; index < count; ++index) {
        result.add(ir.declaration(specs[index]));
        if (index + 1 < count) {
            result.new_lines();
        }
    }
    return result.build();
}

auto view_struct(LoweringContext& context,
                 std::string name,
                 std::string const& view_name,
                 std::string const& const_view_name,
                 std::span<ResolvedMemberId const> members,
                 DeclarationHandle schema,
                 bool const_only,
                 std::span<FunctionId const> functions,
                 LoweredSoa const& api) -> NodeId {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    NodeListBuilder nodes{ir};
    nodes.add(ir.using_declaration(ir.strings.intern("View"), ir.types.intern(view_name)), 1)
        .add(ir.using_declaration(ir.strings.intern("ConstView"), ir.types.intern(const_view_name)),
             2);
    if (context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)]
            .has_value()) {
        nodes
            .append(soa_equivalent_nodes(
                context,
                *context.declarations.soas.equivalent_type[context.declarations.soas.index(schema)],
                members))
            .new_lines(2);
    }
    if (!const_only) {
        if (auto set{soa_set_function(context, schema, members, true)}; set.has_value()) {
            nodes.add(ir.header_function(*set), 2);
        }
    }
    nodes.append(api.header).new_lines(1);
    nodes.add(column_apply_arrays_function(ir, column_names(context, members)), 2)
        .append(declaration_nodes(context, functions))
        .new_lines(2);
    for (std::size_t index{0}; index < members.size(); ++index) {
        nodes.add(ir.instance_member(
            const_only ? resolved_store.const_view_type[resolved_store.index(members[index])]
                       : resolved_store.view_type[resolved_store.index(members[index])],
            resolved_store.name[resolved_store.index(members[index])]));
        if (index + 1 < members.size()) {
            nodes.new_lines();
        }
    }
    auto const created_record{
        ir.record(ir.strings.intern(std::move(name)), nodes.build(), RecordKind::struct_)};
    ir.set_record_export_specifier(
        created_record,
        context.declarations.soas.export_specifier[context.declarations.soas.index(schema)]);
    return created_record;
}

} // namespace

auto soa_equivalent_nodes(LoweringContext& context,
                          TypeRefId equivalent_reference,
                          std::span<ResolvedMemberId const> members) -> Nodes {
    auto& ir{context.ir};
    auto const equivalent{context.resolve(equivalent_reference)};
    auto const index{ir.parameter("int32 const", "index")};
    std::vector<Expr> values;
    for (auto const member : members) {
        values.push_back(equivalent_expression(context, member, ir.named("index")));
    }
    auto function = [&](std::string_view name, Nodes body) {
        auto const created{ir.function(name, "auto")};
        ir.set_function_parameters(created, {index});
        ir.set_function_body(created, body);
        ir.set_function_return_type(created, equivalent, ReturnSyntax::trailing);
        ir.set_function_receiver(created, FunctionReceiver::const_);
        ir.function_flags(created).set_is_inline(true);
        ir.function_flags(created).set_define_in_header(true);
        return ir.header_function(created);
    };
    NodeListBuilder result{ir};
    result.add(ir.using_declaration(ir.strings.intern("equivalent_type"), equivalent), 2)
        .add(function("operator[]", {ir.return_stmt(ir.init_list(values))}), 1)
        .add(function(
            "at",
            {
                ir.expr_stmt(ir.call(ir.named("validate_array_sizes"))),
                ir.expr_stmt(ir.call(
                    ir.named("check", {ir.types.dependency("check", "CoreMinimal.h")}),
                    {ir.binary(
                        BinaryOperator::greater_equal, ir.named("index"), ir.literal("0"))})),
                ir.expr_stmt(ir.call(ir.named("check"),
                                     {ir.binary(BinaryOperator::less,
                                                ir.named("index"),
                                                ir.call(ir.named("num")))})),
                ir.return_stmt(ir.subscript(
                    ir.parenthesized(ir.unary(UnaryOperator::dereference, ir.named("this"))),
                    ir.named("index"))),
            }));
    return result.build();
}

auto soa_view_functions(LoweringContext& context,
                        std::span<ResolvedMemberId const> members,
                        bool const_only) -> std::vector<FunctionId> {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    std::vector<FunctionId> result;
    auto add = [&](std::string_view name,
                   CppTypeId return_type,
                   std::vector<FunctionParameterId> parameters,
                   Nodes body,
                   bool const is_const = false,
                   bool const is_noexcept = false,
                   ReturnSyntax syntax = ReturnSyntax::trailing) {
        auto const created_function{ir.function(name, return_type)};
        ir.set_function_parameters(created_function, parameters);
        ir.set_function_body(created_function, body);
        ir.set_function_return_type(created_function, return_type, syntax);
        ir.set_function_receiver(
            created_function, is_const ? FunctionReceiver::const_ : FunctionReceiver::unqualified);
        if (is_noexcept) {
            ir.set_function_noexcept(created_function);
        }
        result.push_back(created_function);
    };
    auto return_call = [&](std::string_view name, std::vector<Expr> arguments) -> Nodes {
        return {ir.return_stmt(ir.call(ir.named(name), arguments))};
    };

    auto const offset{ir.named("offset")};
    auto const count{ir.named("count")};
    auto const offset_parameter{ir.parameter("int32 const", "offset")};
    auto const count_parameter{ir.parameter("int32 const", "count")};
    auto const num{ir.call(ir.named("num"))};
    auto add_getters = [&](std::string_view name, std::string_view type, bool is_const) {
        auto const return_type{ir.types.intern(type)};
        add(name, return_type, {}, return_call(name, {ir.literal("0"), num}), is_const);
        std::vector<Expr> values;
        for (auto const member : members) {
            values.push_back(view_expression(context, member, is_const));
        }
        add(name,
            return_type,
            {offset_parameter, count_parameter},
            {ir.return_stmt(ir.init_list(values, return_type))},
            is_const);
    };
    if (!const_only) {
        add_getters("get_view", "View", false);
    }

    add_getters("get_view", "ConstView", true);
    add_getters("get_const_view", "ConstView", true);
    add("num",
        ir.types.intern("int32"),
        {},
        {ir.return_stmt(ir.call(
            ir.named("ioj::num", {ir.types.dependency("ioj::num", "SandboxCore/container_ops.h")}),
            {ir.named(resolved_store.name[resolved_store.index(members.front())])}))},
        true,
        true);
    add("is_empty",
        ir.types.intern("bool"),
        {},
        {ir.return_stmt(ir.binary(BinaryOperator::equal, num, ir.literal("0")))},
        true,
        true);
    add("validate_array_sizes",
        ir.types.intern("void"),
        {},
        {ir.expr_stmt(ir.call(
            ir.named("ioj::soa_ops::validate_array_sizes",
                     {ir.types.dependency("ioj::soa_ops", "SandboxCore/soa_storage_ops.h")}),
            {ir.unary(UnaryOperator::dereference, ir.named("this"))}))},
        true,
        false,
        ReturnSyntax::direct);
    auto add_slices = [&](std::string_view type, bool is_const) {
        auto const return_type{ir.types.intern(type)};
        add("slice",
            return_type,
            {offset_parameter, count_parameter},
            return_call("get_view", {offset, count}),
            is_const);
        add("left",
            return_type,
            {count_parameter},
            return_call("slice", {ir.literal("0"), count}),
            is_const);
        add("right",
            return_type,
            {count_parameter},
            return_call("slice", {ir.binary(BinaryOperator::subtract, num, count), count}),
            is_const);
    };
    if (!const_only) {
        add_slices("View", false);
    }
    add_slices("ConstView", true);
    return result;
}

auto soa_view_struct_nodes(LoweringContext& context,
                           DeclarationHandle schema,
                           std::span<ResolvedMemberId const> members,
                           std::string const& view_name,
                           std::string const& const_view_name,
                           SoaFunctions const& functions,
                           LoweredSoa const& const_api,
                           LoweredSoa const& mutable_api,
                           std::map<IdentifierId, DeclarationHandle> const*) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    return result.add(ir.forward_declaration(ir.strings.intern(view_name)), 1)
        .add(ir.forward_declaration(ir.strings.intern(const_view_name)), 2)
        .add(view_struct(context,
                         const_view_name,
                         view_name,
                         const_view_name,
                         members,
                         schema,
                         true,
                         functions.const_view,
                         const_api),
             2)
        .add(view_struct(context,
                         view_name,
                         view_name,
                         const_view_name,
                         members,
                         schema,
                         false,
                         functions.mutable_view,
                         mutable_api),
             2)
        .build();
}

auto soa_storage_view_nodes(LoweringContext& context,
                            std::span<ResolvedMemberId const> members,
                            std::span<FunctionId const> const functions) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    auto const columns{column_names(context, members)};
    return result.add(column_apply_arrays_function(ir, columns), 2)
        .add(column_apply_array_pairs_function(ir, columns), 2)
        .append(declaration_nodes(context, functions))
        .build();
}

} // namespace lispb::codegen::detail
