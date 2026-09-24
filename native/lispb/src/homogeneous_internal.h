#pragma once

#include "lowering.h"

namespace codegen::detail {

auto homogeneous_view_nodes(HomogeneousLayoutSchema const& layout, TypeRegistry const& types)
    -> Nodes;
auto homogeneous_storage_node(HomogeneousLayoutSchema const& layout,
                              HomogeneousValueSchema const& value,
                              TypeRegistry const& types) -> Node;

} // namespace codegen::detail
