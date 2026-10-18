#include "fixed_soa_internal.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/binary_operator.h>
#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
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
    -> std::string {
    auto& fixed_member_store{context.ir.lowering.fixed_member_layouts};
    auto const fixed_member_store_index{fixed_member_store.index(member)};
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    if (resolved_store
            .kind[resolved_store.index(fixed_member_store.member[fixed_member_store_index])] ==
        SoaMemberKind::array) {
        return std::string{is_const ? "TConstArrayView<" : "TArrayView<"} +
               std::string{ir.types.spelling(resolved_store.element_type[resolved_store.index(
                   fixed_member_store.member[fixed_member_store_index])])} +
               ">{" +
               context.text(resolved_store.name[resolved_store.index(
                   fixed_member_store.member[fixed_member_store_index])]) +
               "_.data() + offset, count}";
    }
    return context.text(resolved_store.name[resolved_store.index(
               fixed_member_store.member[fixed_member_store_index])]) +
           (is_const ? "_.get_const_view(offset, count)" : "_.get_view(offset, count)");
}

auto fixed_construct_lines(LoweringContext& context,
                           FixedLayoutId layout,
                           std::string const& operation,
                           std::vector<std::string> const& arguments = {})
    -> std::vector<std::string> {
    auto& layout_store{context.ir.lowering.fixed_layouts};
    auto& fixed_member_store{context.ir.lowering.fixed_member_layouts};
    auto& resolved_store{context.ir.lowering.resolved_members};
    std::vector<std::string> result;
    std::size_t argument_index{};
    for (auto const member :
         context.ir.lowering.items(layout_store.members[layout_store.index(layout)])) {
        auto const name{context.text(resolved_store.name[resolved_store.index(
                            fixed_member_store.member[fixed_member_store.index(member)])]) +
                        "_"};
        auto const leaf_count{
            context.ir.lowering.items(fixed_member_store.leaves[fixed_member_store.index(member)])
                .size()};
        std::vector<std::string> member_arguments;
        for (std::size_t index{0}; index < leaf_count && argument_index < arguments.size();
             ++index) {
            member_arguments.push_back(arguments[argument_index++]);
        }
        if (operation == "arguments") {
            result.push_back(name + ".construct_at(index, " + join(member_arguments, ", ") + ");");
        } else if (operation == "default") {
            result.push_back(resolved_store.kind[resolved_store.index(
                                 fixed_member_store.member[fixed_member_store.index(member)])] ==
                                     SoaMemberKind::array
                                 ? name + ".construct_at(index);"
                                 : name + ".default_construct_at(index);");
        } else if (operation == "copy") {
            result.push_back(resolved_store.kind[resolved_store.index(
                                 fixed_member_store.member[fixed_member_store.index(member)])] ==
                                     SoaMemberKind::array
                                 ? name + ".construct_at(index, other." + name + "[other_index]);"
                                 : name + ".copy_construct_at(index, other." + name +
                                       ", other_index);");
        } else if (operation == "move") {
            result.push_back(
                resolved_store.kind[resolved_store.index(
                    fixed_member_store.member[fixed_member_store.index(member)])] ==
                        SoaMemberKind::array
                    ? name + ".construct_at(index, MoveTemp(other." + name + "[other_index]));"
                    : name + ".move_construct_at(index, other." + name + ", other_index);");
        } else if (operation == "view") {
            result.push_back(
                resolved_store.kind[resolved_store.index(
                    fixed_member_store.member[fixed_member_store.index(member)])] ==
                        SoaMemberKind::array
                    ? name + ".construct_at(index, source." +
                          context.text(resolved_store.name[resolved_store.index(
                              fixed_member_store.member[fixed_member_store.index(member)])]) +
                          "[source_index]);"
                    : name + ".construct_from_view_at(index, source." +
                          context.text(resolved_store.name[resolved_store.index(
                              fixed_member_store.member[fixed_member_store.index(member)])]) +
                          ", source_index);");
        }
    }
    return result;
}

} // namespace

auto fixed_storage_node(LoweringContext& context, FixedLayoutId layout) -> NodeId {
    auto& layout_store{context.ir.lowering.fixed_layouts};
    auto const layout_store_index{layout_store.index(layout)};
    auto& fixed_member_store{context.ir.lowering.fixed_member_layouts};
    auto& resolved_store{context.ir.lowering.resolved_members};
    auto& ir{context.ir};
    auto const& schema{layout_store.schema[layout_store_index]};
    auto const& storage_name{context.text(
        context.declarations.fixed_soas.storage_name[context.declarations.fixed_soas.index(
            *context.declarations.soas.fixed[context.declarations.soas.index(schema)])])};
    std::vector<TemplateParameterId> template_parameters;
    std::vector<FunctionParameterId> function_parameters;
    std::vector<std::string> forwarded;
    for (std::size_t index{0};
         index < context.ir.lowering.items(layout_store.leaves[layout_store_index]).size();
         ++index) {
        auto const argument{fixed_leaf_argument(
            context, context.ir.lowering.items(layout_store.leaves[layout_store_index])[index])};
        template_parameters.push_back(
            ir.type_template_parameter(ir.strings.intern("TArg" + std::to_string(index))));
        function_parameters.push_back(
            ir.parameter(ir.types.intern("TArg" + std::to_string(index) + "&&"),
                         ir.strings.intern("new_" + argument)));
        forwarded.push_back("std::forward<TArg" + std::to_string(index) + ">(new_" + argument +
                            ")");
    }
    std::vector<std::string> mutable_views;
    std::vector<std::string> const_views;
    std::vector<std::string> copy_assign;
    std::vector<std::string> move_assign;
    for (auto const member : context.ir.lowering.items(layout_store.members[layout_store_index])) {
        mutable_views.push_back(fixed_member_view(context, member, false));
        const_views.push_back(fixed_member_view(context, member, true));
        auto const name{context.text(resolved_store.name[resolved_store.index(
                            fixed_member_store.member[fixed_member_store.index(member)])]) +
                        "_"};
        copy_assign.push_back(
            resolved_store.kind[resolved_store.index(
                fixed_member_store.member[fixed_member_store.index(member)])] ==
                    SoaMemberKind::array
                ? name + "[dst_index] = source." +
                      context.text(resolved_store.name[resolved_store.index(
                          fixed_member_store.member[fixed_member_store.index(member)])]) +
                      "[source_index];"
                : name + ".copy_assign_from_view_at(dst_index, source." +
                      context.text(resolved_store.name[resolved_store.index(
                          fixed_member_store.member[fixed_member_store.index(member)])]) +
                      ", source_index);");
        move_assign.push_back(
            resolved_store.kind[resolved_store.index(
                fixed_member_store.member[fixed_member_store.index(member)])] ==
                    SoaMemberKind::array
                ? name + "[dst_index] = MoveTemp(other." + name + "[source_index]);"
                : name + ".move_assign_at(dst_index, other." + name + ", source_index);");
    }
    std::vector<std::string> destroy;
    for (auto iterator{
             context.ir.lowering.items(layout_store.members[layout_store_index]).rbegin()};
         iterator != context.ir.lowering.items(layout_store.members[layout_store_index]).rend();
         ++iterator) {
        destroy.push_back(context.text(resolved_store.name[resolved_store.index(
                              fixed_member_store.member[fixed_member_store.index(*iterator)])]) +
                          "_.destroy_at(index);");
    }
    auto function = [&ir](std::string_view name,
                          CppTypeId return_type,
                          std::vector<FunctionParameterId> parameters,
                          std::string body,
                          FunctionReceiver receiver = FunctionReceiver::unqualified,
                          ReturnSyntax return_syntax = ReturnSyntax::direct,
                          std::vector<TypeDependencyId> dependencies = {},
                          TemplateId function_template = {}) -> FunctionId {
        auto const created_function{ir.function(ir.strings.intern(name), return_type)};
        ir.set_function_return_type(created_function, return_type, return_syntax);
        ir.set_function_receiver(created_function, receiver);
        ir.set_function_parameters(created_function, parameters);
        ir.set_function_body(created_function, {ir.raw(body, dependencies)});
        ir.function_flags(created_function).set_is_inline(true);
        ir.function_flags(created_function).set_define_in_header(true);
        ir.set_function_template(created_function, function_template);
        return created_function;
    };

    NodeListBuilder children{ir};
    children
        .add(ir.using_declaration(ir.strings.intern("View"), ir.types.intern(context.optional_text(context.declarations.soas.view_name[context.declarations.soas.index(schema)])
                                     .value_or(context.text(context.declarations.soas.name[context.declarations.soas.index(schema)]) + "View"))),
             1)
        .add(ir.using_declaration(ir.strings.intern("ConstView"), ir.types.intern(context.optional_text(context.declarations.soas.const_view_name[context.declarations.soas.index(schema)])
                                     .value_or(context.text(context.declarations.soas.name[context.declarations.soas.index(schema)]) + "ConstView"))),
             2)
        .add(ir.header_function(function("get_view",
            ir.types.intern("View"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("offset")),
                       ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
            "return View{" + join(mutable_views, ", ") + "};",
            FunctionReceiver::unqualified,
            ReturnSyntax::trailing,
            {ir.types.dependency("TArrayView", "Containers/ArrayView.h")})),
             1)
        .add(ir.header_function(function("get_const_view",
            ir.types.intern("ConstView"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("offset")),
                       ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("count"))},
            "return ConstView{" + join(const_views, ", ") + "};",
            FunctionReceiver::const_,
            ReturnSyntax::trailing,
            {ir.types.dependency("TArrayView", "Containers/ArrayView.h")})),
             2)
        .add(ir.header_function(function("construct_at",
            ir.types.intern("void"),
            [&] {
                     auto parameters{function_parameters};
                     parameters.insert(parameters.begin(),
                                       ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("index")));
                     return parameters;
                 }(),
            join_lines(fixed_construct_lines(context, layout, "arguments", forwarded)),
            FunctionReceiver::unqualified,
            ReturnSyntax::direct,
            {ir.types.dependency("std::forward", "utility")},
            ir.template_declaration(template_parameters))),
             2)
        .add(ir.header_function(function("default_construct_at",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("index"))},
            join_lines(fixed_construct_lines(context, layout, "default")))),
             2)
        .add(ir.header_function(function("copy_construct_at",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("index")),
                       ir.parameter(ir.types.intern(storage_name + " const&"), ir.strings.intern("other")),
                       ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("other_index"))},
            join_lines(fixed_construct_lines(context, layout, "copy")))),
             2)
        .add(ir.header_function(function("move_construct_at",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("index")),
                       ir.parameter(ir.types.intern(storage_name + "&"), ir.strings.intern("other")),
                       ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("other_index"))},
            join_lines(fixed_construct_lines(context, layout, "move")),
            FunctionReceiver::unqualified,
            ReturnSyntax::direct,
            {ir.types.dependency("MoveTemp", "Templates/UnrealTemplate.h")})),
             2)
        .add(ir.header_function(function("construct_from_view_at",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("index")),
                       ir.parameter(ir.types.intern("SourceView const&"), ir.strings.intern("source")),
                       ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("source_index"))},
            join_lines(fixed_construct_lines(context, layout, "view")),
            FunctionReceiver::unqualified,
            ReturnSyntax::direct,
            {},
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("SourceView"))}))),
             2)
        .add(ir.header_function(function("copy_assign_from_view_at",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("dst_index")),
                       ir.parameter(ir.types.intern("SourceView const&"), ir.strings.intern("source")),
                       ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("source_index"))},
            join_lines(copy_assign),
            FunctionReceiver::unqualified,
            ReturnSyntax::direct,
            {},
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("SourceView"))}))),
             2)
        .add(ir.header_function(function("move_assign_at",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("dst_index")),
                       ir.parameter(ir.types.intern(storage_name + "&"), ir.strings.intern("other")),
                       ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("source_index"))},
            join_lines(move_assign),
            FunctionReceiver::unqualified,
            ReturnSyntax::direct,
            {ir.types.dependency("MoveTemp", "Templates/UnrealTemplate.h")})),
             2)
        .add(ir.header_function([&] { auto const created{function("destroy_at",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("int32 const"), ir.strings.intern("index"))},
            join_lines(destroy))}; ir.set_function_noexcept(created); return created; }()),
             2);
    for (std::size_t index{0};
         index < context.ir.lowering.items(layout_store.members[layout_store_index]).size();
         ++index) {
        auto const member{
            context.ir.lowering.items(layout_store.members[layout_store_index])[index]};
        auto dependencies{std::vector<TypeDependencyId>{
            ir.types.dependency("ioj::TFixedStorage", "ioj/fixed_storage.h")}};
        dependencies.insert(dependencies.end(),
                            ir.types
                                .dependencies(resolved_store.element_type[resolved_store.index(
                                    fixed_member_store.member[fixed_member_store.index(member)])])
                                .begin(),
                            ir.types
                                .dependencies(resolved_store.element_type[resolved_store.index(
                                    fixed_member_store.member[fixed_member_store.index(member)])])
                                .end());
        children.add(ir.instance_member(
            ir.types.intern(fixed_storage_type(context, member), std::move(dependencies)),
            ir.strings.intern(context.text(resolved_store.name[resolved_store.index(
                                  fixed_member_store.member[fixed_member_store.index(member)])]) +
                              "_")));
        if (index + 1 <
            context.ir.lowering.items(layout_store.members[layout_store_index]).size()) {
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
