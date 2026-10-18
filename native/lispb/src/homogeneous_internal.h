#pragma once

#include "lowering.h"
#include "lowering_context.h"

#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/homogeneous_layout_schemas.h>
#include <codegen/schema/homogeneous_value_schemas.h>

namespace lispb::codegen::detail {

auto homogeneous_view_nodes(LoweringContext& context, DeclarationHandle layout) -> Nodes;
auto homogeneous_storage_node(LoweringContext& context,
                              DeclarationHandle layout,
                              HomogeneousValueId value,
                              FunctionId permutation) -> NodeId;

} // namespace lispb::codegen::detail
