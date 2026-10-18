#include "homogeneous_internal.h"
#include "lowering_context.h"
#include "lowering_utils.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
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
#include <codegen/schema/declarations.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cstddef>
#include <optional>
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
                          Nodes body,
                          FunctionReceiver receiver = FunctionReceiver::unqualified,
                          ReturnSyntax return_syntax = ReturnSyntax::direct,
                          TemplateId function_template = {},
                          FunctionFormatting formatting = {}) -> NodeId {
    auto& ir{context.ir};
    auto const created_function{ir.function(ir.strings.intern(name), return_type)};
    ir.set_function_return_type(created_function, return_type, return_syntax);
    ir.set_function_receiver(created_function, receiver);
    ir.set_function_parameters(created_function, parameters);
    ir.set_function_body(created_function, body);
    ir.function_flags(created_function).set_is_inline(true);
    ir.function_flags(created_function).set_define_in_header(true);
    ir.set_function_template(created_function, function_template);
    ir.function_formatting(created_function) = formatting;
    return ir.header_function(created_function);
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
                          std::vector<TypeDependencyId> dependencies = {}) -> NodeId {
    auto& ir{context.ir};
    return homogeneous_function(context,
                                name,
                                return_type,
                                std::move(parameters),
                                Nodes{ir.raw(body, dependencies)},
                                receiver,
                                return_syntax,
                                function_template,
                                formatting);
}

auto same_line_template_formatting() -> FunctionFormatting {
    return FunctionFormatting{
        .template_placement = FunctionTemplatePlacement::same_line,
    };
}

auto substitute_component(std::string value, std::string const& component) -> std::string {
    auto marker{value.find("{}")};
    while (marker != std::string::npos) {
        value.replace(marker, 2, component);
        marker = value.find("{}", marker + component.size());
    }
    return value;
}

auto component_lines(LoweringContext& context,
                     DeclarationHandle layout,
                     std::string const& line_template) -> std::string {
    std::vector<std::string> lines;
    for (auto const component_id : context.declarations.items(
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)])) {
        auto const component{context.text(component_id)};
        lines.push_back(substitute_component(line_template, component));
    }
    return join_lines(lines);
}

auto component_statements(LoweringContext& context,
                          DeclarationHandle layout,
                          std::string const& expression_template) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    for (auto const component_id : context.declarations.items(
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)])) {
        auto const component{context.text(component_id)};
        result.add(ir.expression_statement(
            ir.raw_expression(substitute_component(expression_template, component))));
    }
    return result.build();
}

auto component_assignments(LoweringContext& context,
                           DeclarationHandle layout,
                           std::string const& target_template,
                           std::string const& value_template) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    for (auto const component_id : context.declarations.items(
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)])) {
        auto const component{context.text(component_id)};
        result.add(
            ir.assignment(ir.raw_expression(substitute_component(target_template, component)),
                          ir.raw_expression(substitute_component(value_template, component))));
    }
    return result.build();
}

void add_compact_function(LoweringContext& context,
                          NodeListBuilder& nodes,
                          std::string_view name,
                          CppTypeId return_type,
                          std::vector<FunctionParameterId> parameters,
                          std::string body,
                          FunctionReceiver receiver = FunctionReceiver::unqualified,
                          ReturnSyntax return_syntax = ReturnSyntax::direct,
                          TemplateId function_template = {}) {
    nodes.add(homogeneous_function(context,
                                   name,
                                   return_type,
                                   std::move(parameters),
                                   std::move(body),
                                   receiver,
                                   return_syntax,
                                   function_template,
                                   compact_function_formatting()),
              1);
}

auto homogeneous_pointer_struct(LoweringContext& context,
                                DeclarationHandle layout,
                                std::string name,
                                std::string const& pointer_type) -> NodeId {
    auto& ir{context.ir};
    NodeListBuilder members{ir};
    for (std::size_t index{0};
         index < context.declarations.homogeneous_layouts
                     .components[context.declarations.homogeneous_layouts.index(layout)]
                     .size();
         ++index) {
        members.add(ir.instance_member(
            ir.types.intern(pointer_type),
            context.declarations.items(
                context.declarations.homogeneous_layouts
                    .components[context.declarations.homogeneous_layouts.index(layout)])[index]));
        if (index + 1 < context.declarations.homogeneous_layouts
                            .components[context.declarations.homogeneous_layouts.index(layout)]
                            .size()) {
            members.new_lines();
        }
    }
    return ir.record(ir.strings.intern(std::move(name)), members.build(), RecordKind::struct_);
}

auto homogeneous_storage_prelude_nodes(LoweringContext& context,
                                       DeclarationHandle layout,
                                       CppTypeId const& value_type,
                                       std::optional<CppTypeId> const& equivalent_type,
                                       std::string const& view_name) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    result.add(ir.using_declaration(ir.strings.intern("value_type"), value_type), 1);
    if (equivalent_type.has_value()) {
        result.add(ir.using_declaration(ir.strings.intern("equivalent_type"), *equivalent_type), 1);
    }
    result
        .add(ir.using_declaration(ir.strings.intern("size_type"),
                                  ir.types.intern("TArray<value_type>::SizeType")),
             1)
        .add(ir.using_declaration(ir.strings.intern("View"),
                                  ir.types.intern(view_name + "<value_type>")),
             1)
        .add(ir.using_declaration(ir.strings.intern("ConstView"),
                                  ir.types.intern(view_name + "<value_type const>")),
             2)
        .add(homogeneous_pointer_struct(context, layout, "Data", "value_type*"), 2)
        .add(homogeneous_pointer_struct(context, layout, "ConstData", "value_type const*"), 2);
    for (std::size_t index{0};
         index < context.declarations.homogeneous_layouts
                     .components[context.declarations.homogeneous_layouts.index(layout)]
                     .size();
         ++index) {
        result.add(
            ir.instance_member(ir.types.intern("TArray<value_type>"),
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
    return result.build();
}

auto homogeneous_storage_access_nodes(LoweringContext& context, DeclarationHandle layout) -> Nodes {
    auto& ir{context.ir};
    std::vector<std::string> pointers;
    for (auto const component_id : context.declarations.items(
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)])) {
        auto const component{context.text(component_id)};
        pointers.push_back(component + ".GetData()");
    }
    auto const components{
        join(context,
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)],
             ", ")};
    std::vector<std::string> validation;
    for (std::size_t index{1};
         index < context.declarations.homogeneous_layouts
                     .components[context.declarations.homogeneous_layouts.index(layout)]
                     .size();
         ++index) {
        validation.push_back(
            "check(" +
            context.text(context.declarations.items(
                context.declarations.homogeneous_layouts
                    .components[context.declarations.homogeneous_layouts.index(layout)])[index]) +
            ".Num() == " +
            context.text(
                context.declarations
                    .items(context.declarations.homogeneous_layouts
                               .components[context.declarations.homogeneous_layouts.index(layout)])
                    .front()) +
            ".Num());");
    }

    NodeListBuilder result{ir};
    add_compact_function(context,
                         result,
                         "get_data",
                         ir.types.intern("Data"),
                         {},
                         "return Data{" + join(pointers, ", ") + "};",
                         FunctionReceiver::unqualified,
                         ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "get_data",
                         ir.types.intern("ConstData"),
                         {},
                         "return ConstData{" + join(pointers, ", ") + "};",
                         FunctionReceiver::const_,
                         ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "get_view",
                         ir.types.intern("View"),
                         {},
                         "return View{" + components + "};",
                         FunctionReceiver::unqualified,
                         ReturnSyntax::trailing);
    add_compact_function(
        context,
        result,
        "get_view",
        ir.types.intern("View"),
        {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("offset")),
         ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count"))},
        "return get_view().slice(offset, count);",
        FunctionReceiver::unqualified,
        ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "get_view",
                         ir.types.intern("ConstView"),
                         {},
                         "return ConstView{" + components + "};",
                         FunctionReceiver::const_,
                         ReturnSyntax::trailing);
    add_compact_function(
        context,
        result,
        "get_view",
        ir.types.intern("ConstView"),
        {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("offset")),
         ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count"))},
        "return get_view().slice(offset, count);",
        FunctionReceiver::const_,
        ReturnSyntax::trailing);
    add_compact_function(context,
                         result,
                         "get_const_view",
                         ir.types.intern("ConstView"),
                         {},
                         "return ConstView{" + components + "};",
                         FunctionReceiver::const_,
                         ReturnSyntax::trailing);
    add_compact_function(
        context,
        result,
        "get_const_view",
        ir.types.intern("ConstView"),
        {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("offset")),
         ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count"))},
        "return get_const_view().slice(offset, count);",
        FunctionReceiver::const_,
        ReturnSyntax::trailing);
    add_compact_function(
        context,
        result,
        "apply_arrays",
        ir.types.intern("decltype(auto)"),
        {ir.parameter(ir.types.intern("TFunc&&"), ir.strings.intern("func"))},
        "return std::forward<TFunc>(func)(" + components + ");",
        FunctionReceiver::unqualified,
        ReturnSyntax::trailing,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}));
    add_compact_function(
        context,
        result,
        "apply_arrays",
        ir.types.intern("decltype(auto)"),
        {ir.parameter(ir.types.intern("TFunc&&"), ir.strings.intern("func"))},
        "return std::forward<TFunc>(func)(" + components + ");",
        FunctionReceiver::const_,
        ReturnSyntax::trailing,
        ir.template_declaration({ir.type_template_parameter(ir.strings.intern("TFunc"))}));
    add_compact_function(
        context,
        result,
        "num",
        ir.types.intern("size_type"),
        {},
        "return " +
            context.text(
                context.declarations
                    .items(context.declarations.homogeneous_layouts
                               .components[context.declarations.homogeneous_layouts.index(layout)])
                    .front()) +
            ".Num();",
        FunctionReceiver::const_,
        ReturnSyntax::trailing);
    result.add(homogeneous_function(context,
                                    "validate_array_sizes",
                                    ir.types.intern("void"),
                                    {},
                                    join_lines(validation),
                                    FunctionReceiver::const_,
                                    ReturnSyntax::direct,
                                    {},
                                    {},
                                    {ir.types.dependency("check", "CoreMinimal.h")}),
               1);
    add_compact_function(context,
                         result,
                         "is_empty",
                         ir.types.intern("bool"),
                         {},
                         "return num() == 0;",
                         FunctionReceiver::const_,
                         ReturnSyntax::trailing);
    return result.build();
}

auto homogeneous_storage_copy_nodes(LoweringContext& context, DeclarationHandle layout) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    result.add(
        homogeneous_function(
            context,
            "copy_element",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("dst_i")),
             ir.parameter(ir.types.intern("Other const&"), ir.strings.intern("src")),
             ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("src_i"))},
            component_assignments(context, layout, "{}[dst_i]", "src.{}[src_i]"),
            FunctionReceiver::unqualified,
            ReturnSyntax::trailing,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}),
            same_line_template_formatting()),
        1);
    result.add(
        homogeneous_function(
            context,
            "copy_elements",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("dst_i")),
             ir.parameter(ir.types.intern("Other const&"), ir.strings.intern("src")),
             ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("src_i")),
             ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count"))},
            "for (auto i{0}; i < count; ++i) {\n" +
                component_lines(context, layout, "    {}[dst_i + i] = src.{}[src_i + i];") + "\n}",
            FunctionReceiver::unqualified,
            ReturnSyntax::trailing,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}),
            same_line_template_formatting()),
        1);
    result.add(
        homogeneous_function(
            context,
            "copy_to_tail",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("Other const&"), ir.strings.intern("src"))},
            Nodes{
                ir.variable(ir.types.intern("auto const"),
                            ir.strings.intern("count"),
                            ir.raw_expression("src.num()")),
                ir.expression_statement(ir.raw_expression("check(num() >= count)"),
                                        {ir.types.dependency("check", "CoreMinimal.h")}),
                ir.expression_statement(
                    ir.raw_expression("copy_elements(num() - count, src, 0, count)")),
            },
            FunctionReceiver::unqualified,
            ReturnSyntax::trailing,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}),
            same_line_template_formatting()),
        1);
    result.add(
        homogeneous_function(
            context,
            "append_from",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("Other const&"), ir.strings.intern("other"))},
            component_statements(context, layout, "{}.Append(other.{})"),
            FunctionReceiver::unqualified,
            ReturnSyntax::direct,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Other"))}),
            same_line_template_formatting()),
        1);
    return result.build();
}

auto homogeneous_storage_sort_nodes(LoweringContext& context, FunctionId const permutation)
    -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    result.add(ir.declaration(permutation), 1);
    auto sort_body = [&ir](std::string sort_expression) {
        NodeListBuilder body{ir};
        return body.add(ir.expression_statement(ir.raw_expression("validate_array_sizes()")))
            .add(ir.variable(
                ir.types.intern("auto const"), ir.strings.intern("n"), ir.raw_expression("num()")))
            .add(ir.expression_statement(ir.raw_expression("check(scratch_indices.Num() == n)"),
                                         {ir.types.dependency("check", "CoreMinimal.h")}))
            .add(ir.expression_statement(
                ir.raw_expression("ioj::fill_indices(scratch_indices)"),
                {ir.types.dependency("ioj::fill_indices", "SandboxCore/array_utils.h")}))
            .add(ir.raw(std::move(sort_expression)))
            .add(ir.expression_statement(ir.raw_expression("apply_permutation(scratch_indices)")))
            .build();
    };
    result.add(
        homogeneous_function(
            context,
            "sort",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("Compare&&"), ir.strings.intern("compare")),
             ir.parameter(ir.types.intern("TArrayView<int32>"),
                          ir.strings.intern("scratch_indices"))},
            sort_body("scratch_indices.Sort([this, &compare](int32 const lhs, int32 const rhs) { "
                      "return compare(*this, lhs, rhs); });"),
            FunctionReceiver::unqualified,
            ReturnSyntax::direct,
            ir.template_declaration({ir.type_template_parameter(ir.strings.intern("Compare"))}),
            same_line_template_formatting()),
        1);
    result.add(
        homogeneous_function(
            context,
            "sort",
            ir.types.intern("void"),
            {ir.parameter(ir.types.intern("TArrayView<int32>"),
                          ir.strings.intern("scratch_indices"))},
            sort_body("scratch_indices.Sort([this](int32 const lhs, int32 const rhs) { return "
                      "Compare(*this, lhs, rhs); });"),
            FunctionReceiver::unqualified,
            ReturnSyntax::direct,
            ir.template_declaration({ir.value_template_parameter(ir.types.intern("auto"),
                                                                 ir.strings.intern("Compare"))}),
            same_line_template_formatting()),
        1);
    return result.build();
}

auto homogeneous_storage_array_operation_nodes(LoweringContext& context, DeclarationHandle layout)
    -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    auto add_each = [&](std::string name,
                        std::vector<FunctionParameterId> parameters,
                        std::string const& expression) {
        result.add(homogeneous_function(context,
                                        std::move(name),
                                        ir.types.intern("void"),
                                        std::move(parameters),
                                        component_statements(context, layout, expression),
                                        FunctionReceiver::unqualified,
                                        ReturnSyntax::trailing),
                   1);
    };
    add_each("reset", {}, "{}.Reset()");
    add_each("empty", {}, "{}.Empty()");
    add_each("reserve",
             {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count"))},
             "{}.Reserve(count)");
    add_each("set_num",
             {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count")),
              ir.parameter(ir.types.intern("EAllowShrinking const"),
                           ir.strings.intern("allow_shrinking"))},
             "{}.SetNum(count, allow_shrinking)");
    add_each("set_num_uninitialised",
             {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count"))},
             "{}.SetNumUninitialized(count)");
    add_each("add_uninitialised",
             {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count"))},
             "{}.AddUninitialized(count)");
    add_each("remove_at_swap",
             {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("index")),
              ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count")),
              ir.parameter(ir.types.intern("EAllowShrinking const"),
                           ir.strings.intern("allow_shrinking"))},
             "{}.RemoveAtSwap(index, count, allow_shrinking)");
    add_each("add_zeroed",
             {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count"))},
             "{}.AddZeroed(count)");
    result.add(homogeneous_function(
        context,
        "add_defaulted",
        ir.types.intern("void"),
        {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("count"))},
        component_statements(context, layout, "{}.AddDefaulted(count)"),
        FunctionReceiver::unqualified,
        ReturnSyntax::trailing));
    return result.build();
}

auto homogeneous_storage_value_nodes(LoweringContext& context,
                                     DeclarationHandle layout,
                                     HomogeneousValueId value) -> Nodes {
    auto& ir{context.ir};
    NodeListBuilder result{ir};
    if (context.declarations.homogeneous_values
            .equivalent_type[context.declarations.homogeneous_values.index(value)]
            .has_value()) {
        std::vector<std::string> values;
        for (auto const component_id : context.declarations.items(
                 context.declarations.homogeneous_layouts
                     .components[context.declarations.homogeneous_layouts.index(layout)])) {
            auto const component{context.text(component_id)};
            values.push_back(component + ".GetData()[index]");
        }
        result
            .add(homogeneous_function(
                     context,
                     "operator[]",
                     ir.types.intern("equivalent_type"),
                     {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("index"))},
                     "return {" + join(values, ", ") + "};",
                     FunctionReceiver::const_,
                     ReturnSyntax::trailing),
                 1)
            .add(homogeneous_function(
                     context,
                     "at",
                     ir.types.intern("equivalent_type"),
                     {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("index"))},
                     Nodes{
                         ir.expression_statement(ir.raw_expression("validate_array_sizes()")),
                         ir.expression_statement(ir.raw_expression("check(index >= 0)"),
                                                 {ir.types.dependency("check", "CoreMinimal.h")}),
                         ir.expression_statement(ir.raw_expression("check(index < num())"),
                                                 {ir.types.dependency("check", "CoreMinimal.h")}),
                         ir.return_statement(ir.raw_expression("(*this)[index]")),
                     },
                     FunctionReceiver::const_,
                     ReturnSyntax::trailing),
                 1);
    }
    std::vector<FunctionParameterId> component_parameters;
    for (auto const component_id : context.declarations.items(
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)])) {
        auto const component{context.text(component_id)};
        component_parameters.push_back(
            ir.parameter(ir.types.intern("value_type const"),
                         ir.strings.intern(std::string{component.front()})));
    }
    NodeListBuilder set_body{ir};
    for (auto const component_id : context.declarations.items(
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)])) {
        auto const component{context.text(component_id)};
        set_body.add(ir.assignment(ir.raw_expression(component + "[index]"),
                                   ir.raw_expression(std::string{component.front()})));
    }
    auto set_parameters{component_parameters};
    set_parameters.insert(
        set_parameters.begin(),
        ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("index")));
    result.add(
        homogeneous_function(
            context, "set", ir.types.intern("void"), std::move(set_parameters), set_body.build()),
        1);
    NodeListBuilder component_body{ir};
    auto const& first_component{context.text(
        context.declarations
            .items(context.declarations.homogeneous_layouts
                       .components[context.declarations.homogeneous_layouts.index(layout)])
            .front())};
    component_body.add(
        ir.variable(ir.types.intern("auto const"),
                    ir.strings.intern("index"),
                    ir.raw_expression(first_component + ".Add(" + first_component.front() + ")")));
    for (std::size_t index{1};
         index < context.declarations.homogeneous_layouts
                     .components[context.declarations.homogeneous_layouts.index(layout)]
                     .size();
         ++index) {
        auto const& component{context.text(context.declarations.items(
            context.declarations.homogeneous_layouts
                .components[context.declarations.homogeneous_layouts.index(layout)])[index])};
        component_body.add(ir.expression_statement(
            ir.raw_expression(component + ".Add(" + component.front() + ")")));
    }
    component_body.add(ir.return_statement(ir.raw_expression("index")));
    result.add(homogeneous_function(context,
                                    "add",
                                    ir.types.intern("size_type"),
                                    std::move(component_parameters),
                                    component_body.build(),
                                    FunctionReceiver::unqualified,
                                    ReturnSyntax::trailing),
               1);

    if (context.declarations.homogeneous_values
            .input_types[context.declarations.homogeneous_values.index(value)]
            .empty()) {
        return result.build();
    }

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
    for (auto const input_reference : context.declarations.items(
             context.declarations.homogeneous_values
                 .input_types[context.declarations.homogeneous_values.index(value)])) {
        auto input_type{qualify(ir, context.resolve(input_reference), " const&")};
        result
            .add(homogeneous_function(
                     context,
                     "set",
                     ir.types.intern("void"),
                     {ir.parameter(ir.types.intern("size_type const"), ir.strings.intern("index")),
                      ir.parameter(input_type, ir.strings.intern("value"))},
                     Nodes{ir.expression_statement(
                         ir.raw_expression("set(index, " + join(arguments, ", ") + ")"))}),
                 1)
            .add(homogeneous_function(
                     context,
                     "add",
                     ir.types.intern("size_type"),
                     {ir.parameter(std::move(input_type), ir.strings.intern("value"))},
                     Nodes{ir.return_statement(
                         ir.raw_expression("add(" + join(arguments, ", ") + ")"))},
                     FunctionReceiver::unqualified,
                     ReturnSyntax::trailing),
                 1);
    }
    return result.build();
}

} // namespace

auto homogeneous_storage_node(LoweringContext& context,
                              DeclarationHandle layout,
                              HomogeneousValueId value,
                              FunctionId const permutation) -> NodeId {
    auto& ir{context.ir};
    auto const value_type{
        context.resolve(context.declarations.homogeneous_values
                            .type[context.declarations.homogeneous_values.index(value)])};
    auto const equivalent_type{
        context.declarations.homogeneous_values
                .equivalent_type[context.declarations.homogeneous_values.index(value)]
                .has_value()
            ? std::optional<CppTypeId>{context.resolve(
                  *context.declarations.homogeneous_values
                       .equivalent_type[context.declarations.homogeneous_values.index(value)])}
            : std::nullopt};
    auto const view_name{
        "T" +
        context.text(context.declarations.homogeneous_layouts
                         .name[context.declarations.homogeneous_layouts.index(layout)]) +
        "View"};
    auto const storage_name{
        "F" +
        context.text(context.declarations.homogeneous_layouts
                         .name[context.declarations.homogeneous_layouts.index(layout)]) +
        context.text(context.declarations.homogeneous_values
                         .suffix[context.declarations.homogeneous_values.index(value)])};
    NodeListBuilder children{ir};
    children
        .append(homogeneous_storage_prelude_nodes(
            context, layout, value_type, equivalent_type, view_name))
        .append(homogeneous_storage_access_nodes(context, layout))
        .append(homogeneous_storage_copy_nodes(context, layout))
        .append(homogeneous_storage_sort_nodes(context, permutation))
        .append(homogeneous_storage_value_nodes(context, layout, value))
        .append(homogeneous_storage_array_operation_nodes(context, layout));

    std::vector<TypeDependencyId> dependencies{
        ir.types.dependency("TArray", "Containers/Array.h"),
        ir.types.dependency("TArrayView", "Containers/ArrayView.h"),
        ir.types.dependency("EAllowShrinking", "Containers/AllowShrinking.h"),
        ir.types.dependency("check", "CoreMinimal.h"),
        ir.types.dependency("ioj::fill_indices", "SandboxCore/array_utils.h"),
        ir.types.dependency("std::forward", "utility")};
    dependencies.insert(dependencies.end(),
                        ir.types.dependencies(value_type).begin(),
                        ir.types.dependencies(value_type).end());
    auto const created_record{
        ir.record(ir.strings.intern(storage_name), children.build(), RecordKind::struct_)};
    ir.set_record_export_specifier(
        created_record,
        context.declarations.homogeneous_layouts
            .export_specifier[context.declarations.homogeneous_layouts.index(layout)]);
    ir.set_record_dependencies(created_record, std::move(dependencies));
    return created_record;
}

} // namespace lispb::codegen::detail
