#include "fixed_soa_internal.h"
#include "lowering_context.h"

#include <codegen/ast/indices.h>
#include <codegen/ast/node_list_builder.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/declarations.h>
#include <codegen/schema/fixed_soa_schemas.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>

#include <map>

namespace lispb::codegen::detail {

auto lower_fixed_nodes(LoweringContext& context,
                       DeclarationHandle schema,
                       std::map<StringId, DeclarationHandle> const& schemas,
                       CppTypeId const equivalent_set_type) -> Nodes {
    auto const layout{build_fixed_layout(context, schema, schemas)};
    auto const fixed{*context.declarations.soas.fixed[context.declarations.soas.index(schema)]};
    NodeListBuilder result{context.ir};
    result.add(fixed_storage_node(context, layout));
    for (auto const container : context.declarations.items(
             context.declarations.fixed_soas
                 .containers[context.declarations.fixed_soas.index(fixed)])) {
        result.new_lines(2).add(
            fixed_container_node(context, layout, context.text(container), equivalent_set_type));
    }
    return result.build();
}

} // namespace lispb::codegen::detail
