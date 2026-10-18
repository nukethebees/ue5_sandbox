#pragma once

#include <codegen/ast/node_id.h>
#include <codegen/ast/nodes.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <lispb/indices.h>

#include <map>

namespace lispb::codegen {
class NodeListBuilder;
}

namespace lispb::codegen::detail {

struct LoweringContext;

auto emit_single_allocation_layout(LoweringContext& context, SingleAllocationModelId model)
    -> Nodes;
auto emit_single_allocation_views(LoweringContext& context,
                                  SingleAllocationModelId model,
                                  NodeListBuilder& source,
                                  std::map<IdentifierId, DeclarationHandle> const& schemas)
    -> Nodes;
auto emit_single_allocation_container(LoweringContext& context,
                                      SingleAllocationModelId model,
                                      NodeListBuilder& source,
                                      std::map<IdentifierId, DeclarationHandle> const& schemas)
    -> NodeId;

}
