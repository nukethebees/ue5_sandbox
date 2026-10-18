#include "lowered_soa.h"
#include "lowering_context.h"
#include "single_allocation_soa_emission.h"
#include "single_allocation_soa_model.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>

#include <map>
#include <string>

namespace lispb::codegen::detail {

auto lower_single_allocation(LoweringContext& context,
                             DeclarationHandle schema,
                             std::map<IdentifierId, DeclarationHandle> const& schemas,
                             SoaBackend const backend) -> LoweredSoa {
    auto& ir{context.ir};
    auto model{build_single_allocation_model(context, schema, schemas, backend)};
    NodeListBuilder result{ir};
    NodeListBuilder source{ir};

    result.append(emit_single_allocation_layout(context, model)).new_lines(2);
    result.append(emit_single_allocation_views(context, model, source, schemas)).new_lines(1);
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
    result.add(emit_single_allocation_container(context, model, source, schemas));

    return {result.build(), source.build()};
}

} // namespace lispb::codegen::detail
