#include "declaration_emission.h"
#include "homogeneous_internal.h"
#include "lowering_context.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <cstddef>

namespace lispb::codegen::detail {
namespace {

auto homogeneous_permutation_function(LoweringContext& context, DeclarationHandle layout)
    -> FunctionId {
    auto& ir{context.ir};
    NodeListBuilder body{ir};
    body.add(ir.expression_statement(ir.raw_expression("validate_array_sizes()")))
        .add(ir.expression_statement(ir.raw_expression("check(indices.Num() == num())"),
                                     {ir.types.dependency("check", "CoreMinimal.h")}));
    for (auto const component_id : context.declarations.items(
             context.declarations.homogeneous_layouts
                 .components[context.declarations.homogeneous_layouts.index(layout)])) {
        auto const component{context.text(component_id)};
        body.add(ir.expression_statement(
            ir.raw_expression("ioj::apply_permutation(" + component + ", indices)"),
            {ir.types.dependency("ioj::apply_permutation", "SandboxCore/soa_permutation.h")}));
    }
    auto const created_function{
        ir.function(ir.strings.intern("apply_permutation"), ir.types.intern("void"))};
    ir.set_function_parameters(
        created_function,
        {ir.parameter(
            ir.types.intern("TArrayView<int32>",
                            {ir.types.dependency("TArrayView", "Containers/ArrayView.h")}),
            ir.strings.intern("indices"))});
    ir.set_function_body(created_function, body.build());
    return created_function;
}

auto lower_homogeneous_impl(LoweringContext& context, DeclarationHandle layout)
    -> DeclarationEmission {
    auto& ir{context.ir};
    NodeListBuilder header_definitions{ir};
    NodeListBuilder source_definitions{ir};
    bool has_source_definition{};
    header_definitions.append(homogeneous_view_nodes(context, layout));
    auto const permutation{homogeneous_permutation_function(context, layout)};
    for (std::size_t value_index{0};
         value_index < context.declarations.homogeneous_layouts
                           .value_types[context.declarations.homogeneous_layouts.index(layout)]
                           .size();
         ++value_index) {
        auto const& value{context.declarations.items(
            context.declarations.homogeneous_layouts
                .value_types[context.declarations.homogeneous_layouts.index(layout)])[value_index]};
        header_definitions.new_lines(2).add(
            homogeneous_storage_node(context, layout, value, permutation));

        if (has_source_definition) {
            source_definitions.new_lines(2);
        }
        source_definitions.add(ir.definition(
            permutation,
            ir.store_text(
                "F" +
                context.text(context.declarations.homogeneous_layouts
                                 .name[context.declarations.homogeneous_layouts.index(layout)]) +
                context.text(context.declarations.homogeneous_values
                                 .suffix[context.declarations.homogeneous_values.index(value)]))));
        has_source_definition = true;
    }
    return {.header = header_definitions.build(), .source = source_definitions.build()};
}

} // namespace

auto lower_homogeneous(LoweringContext& context, DeclarationHandle schema) -> DeclarationEmission {
    return lower_homogeneous_impl(context, schema);
}

} // namespace lispb::codegen::detail
