#pragma once

#include "lowering.h"

namespace codegen::detail {

auto homogeneous_view_node(HomogeneousLayoutSchema const& layout) -> Node;
auto homogeneous_storage_node(HomogeneousLayoutSchema const& layout,
                              HomogeneousValueSchema const& value,
                              std::map<std::string, CppType> const& types) -> Node;

} // namespace codegen::detail
