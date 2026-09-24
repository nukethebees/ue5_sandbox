#pragma once

#include "fixed_layout.h"
#include "soa_internal.h"

#include <set>

namespace codegen::detail {

auto build_soa_layout(SoaSchema const& schema,
                      std::map<std::string, SoaSchema const*> const& schemas,
                      TypeRegistry const& types,
                      bool fixed,
                      std::vector<std::string> const& prefix = {},
                      std::set<std::string> ancestors = {}) -> FixedLayout;

auto build_fixed_layout(SoaSchema const& schema,
                        std::map<std::string, SoaSchema const*> const& schemas,
                        TypeRegistry const& types,
                        std::vector<std::string> const& prefix = {},
                        std::set<std::string> ancestors = {}) -> FixedLayout;
auto fixed_leaf_argument(FixedLeaf const& leaf) -> std::string;
auto fixed_storage_node(FixedLayout const& layout) -> Node;
auto fixed_container_node(FixedLayout const& layout,
                          std::string const& name,
                          TypeRegistry const& types) -> Node;
} // namespace codegen::detail
