#pragma once

#include <codegen/ast/node_id.h>
#include <codegen/ast/nodes.h>
#include <codegen/schema/declaration_handle.h>
#include <lispb/indices.h>

namespace lispb::codegen {
class NodeListBuilder;
}

namespace lispb::codegen::detail {

struct LoweringContext;
struct SingleAllocationModel;

auto emit_single_allocation_layout(LoweringContext& ctx, SingleAllocationModel const& model)
    -> Nodes;
auto emit_resolved_views(LoweringContext& ctx,
                         SingleAllocationModel const& model,
                         NodeListBuilder& source,
                         NodeListBuilder& declarations) -> Nodes;
auto emit_compact_views(LoweringContext& ctx,
                        SingleAllocationModel const& model,
                        NodeListBuilder& source) -> Nodes;
auto emit_single_allocation_container(LoweringContext& ctx,
                                      SingleAllocationModel const& model,
                                      NodeListBuilder& source) -> NodeId;

}
