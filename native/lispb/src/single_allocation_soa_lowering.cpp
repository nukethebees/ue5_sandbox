#include "lowered_soa.h"
#include "lowering_context.h"
#include "single_allocation_soa_emission.h"
#include "single_allocation_soa_model.h"

#include <codegen/ast/cpp_ir.h>
#include <codegen/ast/cpp_types.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>

#include <map>
#include <string>

namespace lispb::codegen::detail {

auto lower_single_allocation(LoweringContext& context, DeclarationHandle schema) -> LoweredSoa {
    auto& ir{context.ir};
    auto model{build_single_allocation_model(context, schema)};
    NodeListBuilder result{ir};
    NodeListBuilder source{ir};

    result.append(emit_single_allocation_layout(context, model)).new_lines(2);
    result.append(emit_single_allocation_views(context, model, source)).new_lines(1);
    result.add(emit_single_allocation_container(context, model, source));

    return {result.build(), source.build()};
}

} // namespace lispb::codegen::detail
