#include "homogeneous_internal.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/expr.h>
#include <codegen/ast/function_body_layout.h>
#include <codegen/ast/function_flags.h>
#include <codegen/ast/function_formatting.h>
#include <codegen/ast/function_receiver.h>
#include <codegen/ast/function_template_placement.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/ast/return_syntax.h>
#include <codegen/ast/template_id.h>
#include <codegen/ast/unary_operator.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace lispb::codegen::detail {
namespace {

auto compact_function_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .body_layout = FunctionBodyLayout::compact,
        .template_placement = FunctionTemplatePlacement::same_line,
    };
}

auto homogeneous_function(LoweringContext& context,
                          std::string_view name,
                          CppTypeId return_type,
                          std::vector<FunctionParameterId> parameters,
                          std::string body,
                          FunctionReceiver receiver = FunctionReceiver::unqualified,
                          ReturnSyntax return_syntax = ReturnSyntax::direct,
                          TemplateId function_template = {},
                          FunctionFormatting formatting = {},
                          std::vector<TypeDependencyId> dependencies = {},
                          Expr requires_clause = {}) -> NodeId {
    auto& ir{context.ir};
    auto const created_function{ir.function(name, return_type)};
    ir.set_function_return_type(created_function, return_type, return_syntax);
    ir.set_function_receiver(created_function, receiver);
    ir.set_function_parameters(created_function, parameters);
    ir.set_function_body(created_function, {ir.raw(body, dependencies)});
    ir.function_flags(created_function).set_is_inline(true);
    ir.function_flags(created_function).set_define_in_header(true);
    ir.set_function_template(created_function, function_template);
    ir.set_function_requires_clause(created_function, requires_clause);
    ir.function_formatting(created_function) = formatting;
    return ir.header_function(created_function);
}

} // namespace

auto homogeneous_view_node(LoweringContext& context,
                           DeclarationHandle layout,
                           bool const has_equivalent_type) -> NodeId {
    auto& ir{context.ir};
    auto const offset{ir.parameter("size_type const", "offset")};
    auto const count{ir.parameter("size_type const", "count")};
    auto const index_id{ir.parameter("size_type const", "index")};
    auto const func{ir.parameter("TFunc&&", "func")};
    std::vector<FunctionParameterId> const slice_parameters{offset, count};
    auto const mutable_elements{ir.parenthesized(
        ir.unary(UnaryOperator::logical_not,
                 ir.template_id(ir.named("std::is_const_v",
                                         {ir.types.dependency("std::is_const_v", "type_traits")}),
                                {ir.type_reference(ir.types.intern("T"))})))};
    auto const view_name{
        "T" +
        context.text(context.declarations.homogeneous_layouts
                         .name[context.declarations.homogeneous_layouts.index(layout)]) +
        "View"};
    auto const components{
        join(context,
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)],
             ", ")};
    auto slice_values = [&](std::string const& operation) {
        std::vector<std::string> values;
        for (auto const component_id : context.declarations.items(
                 context.declarations.homogeneous_layouts
                     .components[context.declarations.homogeneous_layouts.index(layout)])) {
            auto const component{context.text(component_id)};
            values.push_back(component + "." + operation);
        }
        return join(values, ", ");
    };
    NodeListBuilder children{ir};
    children
        .add(ir.using_declaration(ir.strings.intern("size_type"),
                                  ir.types.intern("TArrayView<T>::SizeType")),
             1)
        .add(ir.using_declaration(ir.strings.intern("value_type"),
                                  ir.types.intern("std::remove_const_t<T>")),
             1)
        .add(ir.using_declaration(ir.strings.intern("View"), ir.types.intern(view_name + "<T>")), 1)
        .add(ir.using_declaration(ir.strings.intern("ConstView"),
                                  ir.types.intern(view_name + "<value_type const>")),
             has_equivalent_type ? 1 : 2);
    if (has_equivalent_type) {
        children.add(
            ir.using_declaration(
                ir.strings.intern("equivalent_type"),
                ir.types.intern(
                    "typename T" +
                    context.text(
                        context.declarations.homogeneous_layouts
                            .name[context.declarations.homogeneous_layouts.index(layout)]) +
                    "EquivalentType<value_type>::type")),
            2);
    }
    for (std::size_t index{0};
         index < context.declarations.homogeneous_layouts
                     .components[context.declarations.homogeneous_layouts.index(layout)]
                     .size();
         ++index) {
        children.add(
            ir.instance_member(ir.types.intern("TArrayView<T>"),
                               context.declarations.items(
                                   context.declarations.homogeneous_layouts
                                       .components[context.declarations.homogeneous_layouts.index(
                                           layout)])[index]),
            index + 1 < context.declarations.homogeneous_layouts
                            .components[context.declarations.homogeneous_layouts.index(layout)]
                            .size()
                ? 1
                : 2);
    }
    children
        .add(homogeneous_function(context,
                                  "get_view",
                                  ir.types.intern("View"),
                                  {},
                                  "return View{" + components + "};",
                                  FunctionReceiver::unqualified,
                                  ReturnSyntax::trailing,
                                  {},
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function(context,
                                  "get_view",
                                  ir.types.intern("View"),
                                  slice_parameters,
                                  "return get_view().slice(offset, count);",
                                  FunctionReceiver::unqualified,
                                  ReturnSyntax::trailing),
             1)
        .add(homogeneous_function(context,
                                  "get_view",
                                  ir.types.intern("ConstView"),
                                  {},
                                  "return ConstView{" + components + "};",
                                  FunctionReceiver::const_,
                                  ReturnSyntax::trailing,
                                  {},
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function(context,
                                  "get_view",
                                  ir.types.intern("ConstView"),
                                  slice_parameters,
                                  "return get_view().slice(offset, count);",
                                  FunctionReceiver::const_,
                                  ReturnSyntax::trailing),
             1)
        .add(homogeneous_function(context,
                                  "get_const_view",
                                  ir.types.intern("ConstView"),
                                  {},
                                  "return ConstView{" + components + "};",
                                  FunctionReceiver::const_,
                                  ReturnSyntax::trailing,
                                  {},
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function(context,
                                  "get_const_view",
                                  ir.types.intern("ConstView"),
                                  slice_parameters,
                                  "return get_const_view().slice(offset, count);",
                                  FunctionReceiver::const_,
                                  ReturnSyntax::trailing),
             1)
        .add(homogeneous_function(
                 context,
                 "apply_arrays",
                 ir.types.intern("decltype(auto)"),
                 {func},
                 "return std::forward<TFunc>(func)(" + components + ");",
                 FunctionReceiver::unqualified,
                 ReturnSyntax::trailing,
                 ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}),
                 {},
                 {ir.types.dependency("std::forward", "utility")}),
             1)
        .add(homogeneous_function(
                 context,
                 "apply_arrays",
                 ir.types.intern("decltype(auto)"),
                 {func},
                 "return std::forward<TFunc>(func)(" + components + ");",
                 FunctionReceiver::const_,
                 ReturnSyntax::trailing,
                 ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}),
                 {},
                 {ir.types.dependency("std::forward", "utility")}),
             1)
        .add(homogeneous_function(
                 context,
                 "num",
                 ir.types.intern("size_type"),
                 {},
                 "return " +
                     context.text(
                         context.declarations
                             .items(context.declarations.homogeneous_layouts.components
                                        [context.declarations.homogeneous_layouts.index(layout)])
                             .front()) +
                     ".Num();",
                 FunctionReceiver::const_,
                 ReturnSyntax::trailing,
                 {},
                 compact_function_formatting()),
             1)
        .add(homogeneous_function(context,
                                  "is_empty",
                                  ir.types.intern("bool"),
                                  {},
                                  "return num() == 0;",
                                  FunctionReceiver::const_,
                                  ReturnSyntax::trailing,
                                  {},
                                  compact_function_formatting()),
             1)
        .add(homogeneous_function(context,
                                  "slice",
                                  ir.types.intern(view_name),
                                  slice_parameters,
                                  "return " + view_name + "{" +
                                      slice_values("Slice(offset, count)") + "};",
                                  FunctionReceiver::const_,
                                  ReturnSyntax::trailing),
             1)
        .add(homogeneous_function(context,
                                  "left",
                                  ir.types.intern(view_name),
                                  {count},
                                  "return " + view_name + "{" + slice_values("Left(count)") + "};",
                                  FunctionReceiver::const_,
                                  ReturnSyntax::trailing),
             1)
        .add(homogeneous_function(context,
                                  "right",
                                  ir.types.intern(view_name),
                                  {count},
                                  "return " + view_name + "{" + slice_values("Right(count)") + "};",
                                  FunctionReceiver::const_,
                                  ReturnSyntax::trailing));
    std::vector<FunctionParameterId> component_parameters{index_id};
    std::vector<std::string> assignments;
    for (auto const component_id : context.declarations.items(
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)])) {
        auto const component{context.text(component_id)};
        component_parameters.push_back(
            ir.parameter("value_type const", std::string{component.front()}));
        assignments.push_back(component + "[index] = " + component.front() + ";");
    }
    children.new_lines().add(homogeneous_function(context,
                                                  "set",
                                                  ir.types.intern("void"),
                                                  std::move(component_parameters),
                                                  join_lines(assignments),
                                                  FunctionReceiver::const_,
                                                  ReturnSyntax::direct,
                                                  {},
                                                  {},
                                                  {},
                                                  mutable_elements));
    if (has_equivalent_type) {
        static std::vector<std::string> const default_input_members{"X", "Y", "Z"};
        auto const input_member = [&](std::size_t index) {
            return context.declarations.homogeneous_layouts
                           .input_members[context.declarations.homogeneous_layouts.index(layout)]
                           .empty()
                     ? default_input_members[index]
                     : context.text(context.declarations.items(
                           context.declarations.homogeneous_layouts
                               .input_members[context.declarations.homogeneous_layouts.index(
                                   layout)])[index]);
        };
        std::vector<std::string> arguments;
        for (std::size_t index{0};
             index < context.declarations.homogeneous_layouts
                         .components[context.declarations.homogeneous_layouts.index(layout)]
                         .size();
             ++index) {
            arguments.push_back("value." + input_member(index));
        }
        children.new_lines().add(
            homogeneous_function(context,
                                 "set",
                                 ir.types.intern("void"),
                                 {index_id, ir.parameter("equivalent_type const&", "value")},
                                 "set(index, " + join(arguments, ", ") + ");",
                                 FunctionReceiver::const_,
                                 ReturnSyntax::direct,
                                 {},
                                 {},
                                 {},
                                 mutable_elements));
        std::vector<std::string> values;
        for (auto const component_id : context.declarations.items(
                 context.declarations.homogeneous_layouts
                     .components[context.declarations.homogeneous_layouts.index(layout)])) {
            auto const component{context.text(component_id)};
            values.push_back(component + ".GetData()[index]");
        }
        children.new_lines()
            .add(homogeneous_function(context,
                                      "operator[]",
                                      ir.types.intern("equivalent_type"),
                                      {index_id},
                                      "return {" + join(values, ", ") + "};",
                                      FunctionReceiver::const_,
                                      ReturnSyntax::trailing),
                 1)
            .add(homogeneous_function(context,
                                      "at",
                                      ir.types.intern("equivalent_type"),
                                      {index_id},
                                      "check(index >= 0);\ncheck(index < num());\n"
                                      "return (*this)[index];",
                                      FunctionReceiver::const_,
                                      ReturnSyntax::trailing,
                                      {},
                                      {},
                                      {ir.types.dependency("check", "CoreMinimal.h")}));
    }
    auto dependencies{
        std::vector<TypeDependencyId>{ir.types.dependency("TArrayView", "Containers/ArrayView.h"),
                                      ir.types.dependency("std::remove_const_t", "type_traits"),
                                      ir.types.dependency("std::forward", "utility")}};
    if (has_equivalent_type) {
        dependencies.push_back(ir.types.dependency("check", "CoreMinimal.h"));
    }
    auto const created_record{
        ir.record(ir.strings.intern(view_name), children.build(), RecordKind::struct_)};
    ir.set_record_template(
        created_record,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))}));
    ir.set_record_dependencies(created_record, std::move(dependencies));
    return created_record;
}

namespace {

auto equivalent_type_nodes(LoweringContext& context, DeclarationHandle layout) -> Nodes {
    auto& ir{context.ir};
    if (!context.declarations.homogeneous_values
             .equivalent_type[context.declarations.homogeneous_values.index(
                 context.declarations
                     .items(
                         context.declarations.homogeneous_layouts
                             .value_types[context.declarations.homogeneous_layouts.index(layout)])
                     .front())]
             .has_value()) {
        return {};
    }

    auto const trait_name{
        "T" +
        context.text(context.declarations.homogeneous_layouts
                         .name[context.declarations.homogeneous_layouts.index(layout)]) +
        "EquivalentType"};
    NodeListBuilder result{ir};
    result.add(ir.forward_declaration(
                   ir.strings.intern(trait_name),
                   RecordKind::struct_,
                   ir.template_declaration({ir.type_template_parameter(ir.strings.intern("T"))})),
               2);
    for (std::size_t index{0};
         index < context.declarations.homogeneous_layouts
                     .value_types[context.declarations.homogeneous_layouts.index(layout)]
                     .size();
         ++index) {
        auto const& value{context.declarations.items(
            context.declarations.homogeneous_layouts
                .value_types[context.declarations.homogeneous_layouts.index(layout)])[index]};
        auto const value_type{
            context.resolve(context.declarations.homogeneous_values
                                .type[context.declarations.homogeneous_values.index(value)])};
        auto const equivalent_type{context.resolve(
            *context.declarations.homogeneous_values
                 .equivalent_type[context.declarations.homogeneous_values.index(value)])};
        result.add([&] {
            auto const created_record{
                ir.record(ir.template_id(ir.named(trait_name), {ir.type_reference(value_type)}),
                          {ir.using_declaration(ir.strings.intern("type"), equivalent_type)},
                          RecordKind::struct_)};
            ir.set_record_template(created_record, ir.template_specialization());
            return created_record;
        }());
        if (index + 1 < context.declarations.homogeneous_layouts
                            .value_types[context.declarations.homogeneous_layouts.index(layout)]
                            .size()) {
            result.new_lines(2);
        }
    }
    return result.build();
}

} // namespace

auto homogeneous_view_nodes(LoweringContext& context, DeclarationHandle layout) -> Nodes {
    auto& ir{context.ir};
    auto const has_equivalent_type{
        context.declarations.homogeneous_values
            .equivalent_type[context.declarations.homogeneous_values.index(
                context.declarations
                    .items(context.declarations.homogeneous_layouts
                               .value_types[context.declarations.homogeneous_layouts.index(layout)])
                    .front())]
            .has_value()};
    NodeListBuilder result{ir};
    result.append(equivalent_type_nodes(context, layout));
    if (has_equivalent_type) {
        result.new_lines(2);
    }
    return result.add(homogeneous_view_node(context, layout, has_equivalent_type)).build();
}

} // namespace lispb::codegen::detail
