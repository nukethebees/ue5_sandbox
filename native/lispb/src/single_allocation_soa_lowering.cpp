#include "lowered_soa.h"
#include "lowering_context.h"
#include "single_allocation_soa_emission.h"
#include "single_allocation_soa_model.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/schema/declaration_schema.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/strings.h>

#include <map>
#include <string>

namespace lispb::codegen::detail {

auto lower_single_allocation(LoweringContext& context, DeclarationHandle schema) -> LoweredSoa {
    auto& ir{context.ir};
    auto const model{build_single_allocation_model(context, schema)};
    auto const& soas{context.declarations.soas};
    auto const row{soas.index(schema)};
    auto const namespace_name{ir.strings.intern(soa_namespace_name(context.text(soas.name[row])))};
    NodeListBuilder result{ir};
    NodeListBuilder source{ir};
    NodeListBuilder view_source{ir};
    NodeListBuilder shared{ir};
    NodeListBuilder declarations{ir};
    declarations.add(ir.forward_declaration(ir.strings.intern("Schema"))).new_lines(1);
    auto const views{emit_single_allocation_views(context, model, view_source, declarations)};

    shared.append(declarations.build()).new_lines(2);
    shared.append(emit_single_allocation_layout(context, model)).new_lines(2);
    shared.append(views);
    result.add(ir.namespace_node(namespace_name, shared.build()));
    auto const source_views{view_source.build()};
    if (!source_views.empty()) {
        source.add(ir.namespace_node(namespace_name, source_views)).new_lines(2);
    }
    if (soas.layout_only[row]) {
        return {result.build(), source.build()};
    }
    result.new_lines(2);
    result.add(ir.block_comment("****************************************"))
        .new_lines(1)
        .add(ir.line_comment("Owner"))
        .new_lines(1)
        .add(ir.block_comment("****************************************"))
        .new_lines(1);
    result.add(ir.line_comment("Columns:")).new_lines(1);
    for (auto const column :
         ir.lowering.items(ir.lowering.single_allocation_models
                               .columns[ir.lowering.single_allocation_models.index(model)])) {
        result
            .add(ir.line_comment(
                context.text(ir.lowering.single_allocation_columns
                                 .flattened_identifier[ir.lowering.single_allocation_columns.index(
                                     column)]) +
                ": " +
                std::string{ir.types.spelling(
                    ir.lowering.single_allocation_columns
                        .type[ir.lowering.single_allocation_columns.index(column)])}))
            .new_lines(1);
    }
    result.add(emit_single_allocation_container(context, model, source));

    return {result.build(), source.build()};
}

} // namespace lispb::codegen::detail
