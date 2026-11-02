#include "lowered_soa.h"
#include "lowering_context.h"
#include "single_allocation_soa_emission.h"
#include "single_allocation_soa_model.h"

#include <codegen/ast/cpp_ir_builder.h>
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
namespace {

class SingleAllocationLowering : public CppIRBuilder {
  public:
    explicit SingleAllocationLowering(CppIR& ir)
        : CppIRBuilder{ir} {}
    auto lower_single_allocation(LoweringContext& ctx, DeclarationHandle schema) -> LoweredSoa {
        auto const model{build_single_allocation_model(ctx, schema)};
        auto const& soas{ctx.declarations.soas};
        auto const row{soas.index(schema)};
        auto const namespace_name{store_text(soa_namespace_name(ctx.text(soas.name[row])))};
        auto result{node_list()};
        auto source{node_list()};
        auto view_source{node_list()};
        auto shared{node_list()};
        auto declarations{node_list()};
        declarations.add(forward_declaration(store_text("Schema"))).new_lines(1);
        auto const views{emit_single_allocation_views(ctx, model, view_source, declarations)};

        shared.append(declarations.build()).new_lines(2);
        shared.append(emit_single_allocation_layout(ctx, model)).new_lines(2);
        shared.append(views);
        result.add(namespace_node(namespace_name, shared.build()));
        auto const source_views{view_source.build()};
        if (!source_views.empty()) {
            source.add(namespace_node(namespace_name, source_views)).new_lines(2);
        }
        if (soas.layout_only[row]) {
            return {result.build(), source.build()};
        }
        result.new_lines(2);
        result.add(block_comment("****************************************"))
            .new_lines(1)
            .add(line_comment("Owner"))
            .new_lines(1)
            .add(block_comment("****************************************"))
            .new_lines(1);
        result.add(line_comment("Columns:")).new_lines(1);
        auto const& models{lowering_storage().single_allocation_models};
        auto const& columns{lowering_storage().single_allocation_columns};
        for (auto const column : lowering_storage().items(models.columns[models.index(model)])) {
            auto const column_row{columns.index(column)};
            result
                .add(line_comment(ctx.text(columns.flattened_identifier[column_row]) + ": " +
                                  std::string{type_spelling(columns.type[column_row])}))
                .new_lines(1);
        }
        result.add(emit_single_allocation_container(ctx, model, source));

        return {result.build(), source.build()};
    }
};

} // namespace

auto lower_single_allocation(LoweringContext& ctx, DeclarationHandle schema) -> LoweredSoa {
    return SingleAllocationLowering{ctx.ir}.lower_single_allocation(ctx, schema);
}

} // namespace lispb::codegen::detail
