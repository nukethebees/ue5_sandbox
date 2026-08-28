#pragma once

#include "fixed_layout.h"
#include "soa_internal.h"

#include <set>

namespace codegen::detail {

auto build_fixed_layout(SoaSchema const& schema,
                        std::map<std::string, SoaSchema const*> const& schemas,
                        std::map<std::string, CppType> const& types,
                        std::vector<std::string> prefix = {},
                        std::set<std::string> ancestors = {}) -> FixedLayout;
auto fixed_leaf_argument(FixedLeaf const& leaf) -> std::string;
auto fixed_storage_node(FixedLayout const& layout) -> Node;
auto fixed_container_node(FixedLayout const& layout,
                          std::string const& name,
                          std::map<std::string, CppType> const& types) -> Node;
} // namespace codegen::detail
