#pragma once

#include "lowering.h"

namespace codegen::detail {

auto homogeneous_view_nodes(HomogeneousLayoutSchema const& layout,
                            std::map<std::string, CppType> const& types) -> Nodes;
auto homogeneous_storage_node(HomogeneousLayoutSchema const& layout,
                              HomogeneousValueSchema const& value,
                              std::map<std::string, CppType> const& types) -> Node;

} // namespace codegen::detail
