#include "fixed_soa_internal.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/template_id.h>
#include <codegen/ast/template_parameter_id.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/soa_member_kind.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto fixed_storage_type(LoweringContext& context, FixedMemberLayoutId member) -> std::string {
    auto& fixed_member_store{context.ir.lowering.fixed_member_layouts};
    auto const fixed_member_store_index{fixed_member_store.index(member)};
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    if (resolved_store
            .kind[resolved_store.index(fixed_member_store.member[fixed_member_store_index])] ==
        SoaMemberKind::array) {
        return "ioj::TFixedStorage<" +
               std::string{ir.types.spelling(resolved_store.element_type[resolved_store.index(
                   fixed_member_store.member[fixed_member_store_index])])} +
               ", Capacity>";
    }
    return context.text(fixed_member_store.nested_storage_name[fixed_member_store_index]) +
           "<Capacity>";
}

auto fixed_member_view(LoweringContext& context, FixedMemberLayoutId member, bool is_const)
    -> Expr {
    auto& fixed_members{context.ir.lowering.fixed_member_layouts};
    auto& resolved{context.ir.lowering.resolved_members};
    auto const row{resolved.index(fixed_members.member[fixed_members.index(member)])};
    auto& ir{context.ir};
    auto const column{ir.named(context.text(resolved.name[row]) + "_")};
    if (resolved.kind[row] == SoaMemberKind::array) {
        auto const type{
            ir.types.intern(std::string{is_const ? "TConstArrayView<" : "TArrayView<"} +
                                std::string{ir.types.spelling(resolved.element_type[row])} + ">",
                            {ir.types.dependency("TArrayView", "Containers/ArrayView.h")})};
        return ir.init_list({ir.binary(BinaryOperator::add,
                                       ir.call(ir.member_access(column, "data")),
                                       ir.named("offset")),
                             ir.named("count")},
                            type);
    }
    return ir.call(ir.member_access(column, is_const ? "get_const_view" : "get_view"),
                   {ir.named("offset"), ir.named("count")});
}

auto fixed_construct_nodes(LoweringContext& context,
                           FixedLayoutId layout,
                           std::string_view operation,
                           std::span<Expr const> arguments = {}) -> Nodes {
    auto& layouts{context.ir.lowering.fixed_layouts};
    auto& fixed_members{context.ir.lowering.fixed_member_layouts};
    auto& resolved{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    std::size_t argument_index{};
    for (auto const member : context.ir.lowering.items(layouts.members[layouts.index(layout)])) {
        auto const member_row{fixed_members.index(member)};
        auto const row{resolved.index(fixed_members.member[member_row])};
        auto const name{context.text(resolved.name[row])};
        auto const column{ir.named(name + "_")};
        auto const array{resolved.kind[row] == SoaMemberKind::array};
        auto const index{ir.named("index")};
        auto const other_index{ir.named("other_index")};
        std::string_view method{"construct_at"};
        std::vector<Expr> values{index};
        if (operation == "arguments") {
            auto const leaf_count{
                context.ir.lowering.items(fixed_members.leaves[member_row]).size()};
            for (std::size_t leaf{}; leaf < leaf_count && argument_index < arguments.size();
                 ++leaf) {
                values.push_back(arguments[argument_index++]);
            }
        } else if (operation == "default") {
            method = array ? "construct_at" : "default_construct_at";
        } else if (operation == "copy" || operation == "move") {
            auto const other{ir.member_access(ir.named("other"), name + "_")};
            if (array) {
                auto value{ir.subscript(other, other_index)};
                if (operation == "move") {
                    value = ir.call(
                        ir.named("MoveTemp",
                                 {ir.types.dependency("MoveTemp", "Templates/UnrealTemplate.h")}),
                        {value});
                }
                values.push_back(value);
            } else {
                method = operation == "copy" ? "copy_construct_at" : "move_construct_at";
                values.push_back(other);
                values.push_back(other_index);
            }
        } else if (operation == "view") {
            auto const source{ir.member_access(ir.named("source"), name)};
            auto const source_index{ir.named("source_index")};
            if (array) {
                values.push_back(ir.subscript(source, source_index));
            } else {
                method = "construct_from_view_at";
                values.push_back(source);
                values.push_back(source_index);
            }
        }
        result.add(ir.expr_stmt(ir.call(ir.member_access(column, method), values)));
    }
    return result.build();
}

} // namespace

auto fixed_storage_node(LoweringContext& context, FixedLayoutId layout) -> NodeId {
    auto& layout_store{context.ir.lowering.fixed_layouts};
    auto const layout_store_index{layout_store.index(layout)};
    auto& fixed_member_store{context.ir.lowering.fixed_member_layouts};
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    auto const& schema{layout_store.schema[layout_store_index]};
    auto const& schemas{context.declarations.soas};
    auto const schema_row{schemas.index(schema)};
    auto const schema_name{context.text(schemas.name[schema_row])};
    auto const& storage_name{context.text(
        context.declarations.fixed_soas
            .storage_name[context.declarations.fixed_soas.index(*schemas.fixed[schema_row])])};
    std::vector<TemplateParameterId> template_parameters;
    std::vector<FunctionParameterId> function_parameters;
    std::vector<Expr> forwarded;
    for (std::size_t index{0};
         index < context.ir.lowering.items(layout_store.leaves[layout_store_index]).size();
         ++index) {
        auto const argument{fixed_leaf_argument(
            context, context.ir.lowering.items(layout_store.leaves[layout_store_index])[index])};
        template_parameters.push_back(
            ir.type_template_parameter(ir.strings.intern("TArg" + std::to_string(index))));
        function_parameters.push_back(
            ir.parameter("TArg" + std::to_string(index) + "&&", "new_" + argument));
        forwarded.push_back(
            ir.call(ir.template_id(
                        ir.named("std::forward", {ir.types.dependency("std::forward", "utility")}),
                        {ir.type_reference(ir.types.intern("TArg" + std::to_string(index)))}),
                    {ir.named("new_" + argument)}));
    }
    std::vector<Expr> mutable_views;
    std::vector<Expr> const_views;
    Nodes copy_assign;
    Nodes move_assign;
    auto const members{context.ir.lowering.items(layout_store.members[layout_store_index])};
    for (auto const member : members) {
        mutable_views.push_back(fixed_member_view(context, member, false));
        const_views.push_back(fixed_member_view(context, member, true));
        auto const row{
            resolved_store.index(fixed_member_store.member[fixed_member_store.index(member)])};
        auto const name{context.text(resolved_store.name[row])};
        auto const column{ir.named(name + "_")};
        auto const source{ir.member_access(ir.named("source"), name)};
        auto const other{ir.member_access(ir.named("other"), name + "_")};
        auto const destination_index{ir.named("dst_index")};
        auto const source_index{ir.named("source_index")};
        if (resolved_store.kind[row] == SoaMemberKind::array) {
            copy_assign.push_back(ir.assignment(ir.subscript(column, destination_index),
                                                ir.subscript(source, source_index)));
            move_assign.push_back(ir.assignment(
                ir.subscript(column, destination_index),
                ir.call(ir.named("MoveTemp",
                                 {ir.types.dependency("MoveTemp", "Templates/UnrealTemplate.h")}),
                        {ir.subscript(other, source_index)})));
        } else {
            copy_assign.push_back(
                ir.expr_stmt(ir.call(ir.member_access(column, "copy_assign_from_view_at"),
                                     {destination_index, source, source_index})));
            move_assign.push_back(ir.expr_stmt(ir.call(ir.member_access(column, "move_assign_at"),
                                                       {destination_index, other, source_index})));
        }
    }
    Nodes destroy;
    for (auto iterator{members.rbegin()}; iterator != members.rend(); ++iterator) {
        auto const row{
            resolved_store.index(fixed_member_store.member[fixed_member_store.index(*iterator)])};
        auto const column{ir.named(context.text(resolved_store.name[row]) + "_")};
        destroy.push_back(
            ir.expr_stmt(ir.call(ir.member_access(column, "destroy_at"), {ir.named("index")})));
    }
    NodeListBuilder children{ir};
    auto add = [&ir, &children](FunctionId function,
                                std::vector<FunctionParameterId> parameters,
                                Nodes body,
                                int trailing_new_lines = 2) {
        ir.set_function_parameters(function, parameters);
        ir.set_function_body(function, body);
        ir.function_flags(function).set_is_inline(true);
        ir.function_flags(function).set_define_in_header(true);
        children.add(ir.header_function(function), trailing_new_lines);
        return function;
    };
    auto operation = [&](std::string_view name,
                         std::vector<FunctionParameterId> parameters,
                         Nodes body,
                         std::vector<TypeDependencyId> dependencies = {},
                         TemplateId function_template = {}) {
        auto const function{ir.function(name, "void")};
        ir.set_function_dependencies(function, dependencies);
        ir.set_function_template(function, function_template);
        return add(function, std::move(parameters), std::move(body));
    };
    auto const index_parameter{ir.parameter("int32 const", "index")};
    auto const other_index{ir.parameter("int32 const", "other_index")};
    auto const source{ir.parameter("SourceView const&", "source")};
    auto const source_index{ir.parameter("int32 const", "source_index")};
    auto const destination_index{ir.parameter("int32 const", "dst_index")};
    auto const other{ir.parameter(storage_name + "&", "other")};
    auto const source_template{
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("SourceView"))})};
    function_parameters.insert(function_parameters.begin(), index_parameter);

    children
        .add(ir.using_declaration(
                 ir.strings.intern("View"),
                 ir.types.intern(context.optional_text(schemas.view_name[schema_row])
                                     .value_or(schema_name + "View"))),
             1)
        .add(ir.using_declaration(
                 ir.strings.intern("ConstView"),
                 ir.types.intern(context.optional_text(schemas.const_view_name[schema_row])
                                     .value_or(schema_name + "ConstView"))),
             2);
    auto view = [&](std::string_view function_name,
                    std::string_view type,
                    std::span<Expr const> values,
                    FunctionReceiver receiver,
                    int new_lines) {
        auto const return_type{ir.types.intern(type)};
        auto const function{ir.function(function_name, return_type)};
        ir.set_function_return_type(function, return_type, ReturnSyntax::trailing);
        ir.set_function_receiver(function, receiver);
        ir.set_function_dependencies(function,
                                     {ir.types.dependency("TArrayView", "Containers/ArrayView.h")});
        add(function,
            {ir.parameter("int32 const", "offset"), ir.parameter("int32 const", "count")},
            {ir.return_stmt(ir.init_list(values, return_type))},
            new_lines);
    };
    view("get_view", "View", mutable_views, FunctionReceiver::unqualified, 1);
    view("get_const_view", "ConstView", const_views, FunctionReceiver::const_, 2);
    operation("construct_at",
              function_parameters,
              fixed_construct_nodes(context, layout, "arguments", forwarded),
              {ir.types.dependency("std::forward", "utility")},
              ir.template_declaration(template_parameters));
    operation("default_construct_at",
              {index_parameter},
              fixed_construct_nodes(context, layout, "default"));
    operation("copy_construct_at",
              {index_parameter, ir.parameter(storage_name + " const&", "other"), other_index},
              fixed_construct_nodes(context, layout, "copy"));
    operation("move_construct_at",
              {index_parameter, other, other_index},
              fixed_construct_nodes(context, layout, "move"),
              {ir.types.dependency("MoveTemp", "Templates/UnrealTemplate.h")});
    operation("construct_from_view_at",
              {index_parameter, source, source_index},
              fixed_construct_nodes(context, layout, "view"),
              {},
              source_template);
    operation("copy_assign_from_view_at",
              {destination_index, source, source_index},
              copy_assign,
              {},
              source_template);
    operation("move_assign_at",
              {destination_index, other, source_index},
              move_assign,
              {ir.types.dependency("MoveTemp", "Templates/UnrealTemplate.h")});
    ir.set_function_noexcept(operation("destroy_at", {index_parameter}, destroy));
    auto const member_count{members.size()};
    for (std::size_t index{}; index < member_count; ++index) {
        auto const member{members[index]};
        auto const row{
            resolved_store.index(fixed_member_store.member[fixed_member_store.index(member)])};
        auto dependencies{std::vector<TypeDependencyId>{
            ir.types.dependency("ioj::TFixedStorage", "ioj/fixed_storage.h")}};
        auto const type_dependencies{ir.types.dependencies(resolved_store.element_type[row])};
        dependencies.insert(dependencies.end(), type_dependencies.begin(), type_dependencies.end());
        children.add(ir.instance_member(
            ir.types.intern(fixed_storage_type(context, member), std::move(dependencies)),
            ir.strings.intern(context.text(resolved_store.name[row]) + "_")));
        if (index + 1 < member_count) {
            children.new_lines();
        }
    }

    auto const created_record{
        ir.record(ir.strings.intern(storage_name), children.build(), RecordKind::struct_)};
    ir.set_record_template(created_record,
                           ir.template_declaration({ir.value_template_parameter(
                               ir.types.intern("int32"), ir.strings.intern("Capacity"))}));
    ir.set_record_requires_clause(
        created_record,
        ir.parenthesized(
            ir.binary(BinaryOperator::greater_equal, ir.named("Capacity"), ir.literal("0"))));
    return created_record;
}

} // namespace lispb::codegen::detail
