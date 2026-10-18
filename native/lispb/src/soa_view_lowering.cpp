#include "lowered_soa.h"
#include "lowering_context.h"
#include "lowering_utils.h"
#include "soa_functions.h"
#include "soa_internal.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/return_syntax.h>
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
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto view_expression(LoweringContext& context, ResolvedMemberId member, bool const_view)
    -> std::string {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto const resolved_store_index{resolved_store.index(member)};
    auto& ir{context.ir};
    if (resolved_store.kind[resolved_store_index] == SoaMemberKind::nested) {
        return context.text(resolved_store.name[resolved_store_index]) +
               (const_view ? ".get_const_view(offset, count)" : ".get_view(offset, count)");
    }
    auto const type{const_view ? resolved_store.const_view_type[resolved_store_index]
                               : resolved_store.view_type[resolved_store_index]};
    return std::string{ir.types.spelling(type)} + "{" +
           context.text(resolved_store.name[resolved_store_index]) + "}.Slice(offset, count)";
}

auto equivalent_expression(LoweringContext& context,
                           ResolvedMemberId member,
                           std::string const& index) -> std::string {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto const resolved_store_index{resolved_store.index(member)};
    if (resolved_store.kind[resolved_store_index] == SoaMemberKind::array) {
        return context.text(resolved_store.name[resolved_store_index]) + ".GetData()[" + index +
               "]";
    }
    return context.text(resolved_store.name[resolved_store_index]) + "[" + index + "]";
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
    std::vector<std::string> values;
    for (auto const member : members) {
        values.push_back(equivalent_expression(context, member, "index"));
    }
    NodeListBuilder result{ir};
    result.add(ir.using_declaration(ir.strings.intern("equivalent_type"), equivalent), 2)
        .add(ir.header_function([&] {
            auto const created_function{
                ir.function(ir.strings.intern("operator[]"), ir.types.intern("auto"))};
            ir.set_function_parameters(
                created_function,
                {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("index"))});
            ir.set_function_body(
                created_function,
                {ir.return_statement(ir.raw_expression("{" + join(values, ", ") + "}"))});
            ir.set_function_return_type(created_function, equivalent, ReturnSyntax::trailing);
            ir.set_function_receiver(created_function, FunctionReceiver::const_);
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            return created_function;
        }()),
             1)
        .add(ir.header_function([&] {
            auto const created_function{
                ir.function(ir.strings.intern("at"), ir.types.intern("auto"))};
            ir.set_function_parameters(
                created_function,
                {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("index"))});
            ir.set_function_body(
                created_function,
                {
                    ir.expression_statement(ir.raw_expression("validate_array_sizes()")),
                    ir.expression_statement(ir.raw_expression("check("
                                                              "index "
                                                              ">= "
                                                              "0)"),
                                            {ir.types.dependency("check", "CoreMinimal.h")}),
                    ir.expression_statement(ir.raw_expression("check(index < "
                                                              "num())")),
                    ir.return_statement(ir.raw_expression("(*this)[index]")),
                });
            ir.set_function_return_type(created_function, equivalent, ReturnSyntax::trailing);
            ir.set_function_receiver(created_function, FunctionReceiver::const_);
            ir.function_flags(created_function).set_is_inline(true);
            ir.function_flags(created_function).set_define_in_header(true);
            return created_function;
        }()));
    return result.build();
}

auto soa_view_functions(LoweringContext& context,
                        std::span<ResolvedMemberId const> members,
                        bool const_only) -> std::vector<FunctionId> {
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    std::vector<FunctionId> result;
    auto add = [&](std::string name,
                   CppTypeId return_type,
                   std::vector<FunctionParameterId> parameters,
                   std::string body,
                   ReturnSyntax syntax,
                   bool const is_const = false,
                   bool const is_noexcept = false,
                   std::vector<TypeDependencyId> dependencies = {}) {
        result.push_back([&] {
            auto const created_function{
                ir.function(ir.strings.intern(std::move(name)), std::move(return_type))};
            ir.set_function_parameters(created_function, std::move(parameters));
            ir.set_function_body(created_function,
                                 {ir.raw(std::move(body), std::move(dependencies))});
            ir.set_function_return_type(created_function, return_type, syntax);
            ir.set_function_receiver(created_function,
                                     is_const ? FunctionReceiver::const_
                                              : FunctionReceiver::unqualified);
            if (is_noexcept) {
                ir.set_function_noexcept(created_function);
            }
            return created_function;
        }());
    };

    if (!const_only) {
        add("get_view",
            ir.types.intern("View"),
            {},
            "return get_view(0, num());",
            ReturnSyntax::trailing);
        std::vector<std::string> values;
        for (auto const member : members) {
            values.push_back("    " + view_expression(context, member, false) + ",");
        }
        add("get_view",
            ir.types.intern("View"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("offset")),
             ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
            "return View{\n" + join_lines(values) + "\n};",
            ReturnSyntax::trailing);
    }

    add("get_view",
        ir.types.intern("ConstView"),
        {},
        "return get_view(0, num());",
        ReturnSyntax::trailing,
        true);
    std::vector<std::string> const_values;
    for (auto const member : members) {
        const_values.push_back("    " + view_expression(context, member, true) + ",");
    }
    add("get_view",
        ir.types.intern("ConstView"),
        {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("offset")),
         ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
        "return ConstView{\n" + join_lines(const_values) + "\n};",
        ReturnSyntax::trailing,
        true);
    add("get_const_view",
        ir.types.intern("ConstView"),
        {},
        "return get_const_view(0, num());",
        ReturnSyntax::trailing,
        true);
    add("get_const_view",
        ir.types.intern("ConstView"),
        {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("offset")),
         ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
        "return ConstView{\n" + join_lines(const_values) + "\n};",
        ReturnSyntax::trailing,
        true);
    add("num",
        ir.types.intern("int32"),
        {},
        "return ioj::num(" +
            context.text(resolved_store.name[resolved_store.index(members.front())]) + ");",
        ReturnSyntax::trailing,
        true,
        true,
        {ir.types.dependency("ioj::num", "SandboxCore/container_ops.h")});
    add("is_empty",
        ir.types.intern("bool"),
        {},
        "return num() == 0;",
        ReturnSyntax::trailing,
        true,
        true);
    add("validate_array_sizes",
        ir.types.intern("void"),
        {},
        "ioj::soa_ops::validate_array_sizes(*this);",
        ReturnSyntax::direct,
        true,
        false,
        {ir.types.dependency("ioj::soa_ops", "SandboxCore/soa_storage_ops.h")});
    if (!const_only) {
        add("slice",
            ir.types.intern("View"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("offset")),
             ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
            "return get_view(offset, count);",
            ReturnSyntax::trailing);
        add("left",
            ir.types.intern("View"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
            "return slice(0, count);",
            ReturnSyntax::trailing);
        add("right",
            ir.types.intern("View"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
            "return slice(num() - count, count);",
            ReturnSyntax::trailing);
    }
    add("slice",
        ir.types.intern("ConstView"),
        {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("offset")),
         ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
        "return get_view(offset, count);",
        ReturnSyntax::trailing,
        true);
    add("left",
        ir.types.intern("ConstView"),
        {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
        "return slice(0, count);",
        ReturnSyntax::trailing,
        true);
    add("right",
        ir.types.intern("ConstView"),
        {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
        "return slice(num() - count, count);",
        ReturnSyntax::trailing,
        true);
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
