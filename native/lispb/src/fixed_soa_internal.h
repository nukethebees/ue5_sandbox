#pragma once

#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/declaration_handle.h>
#include <lispb/indices.h>

#include <map>
#include <set>
#include <string>

namespace lispb::codegen::detail {

struct LoweringContext;

auto build_soa_layout(LoweringContext& context,
                      DeclarationHandle schema,
                      std::map<IdentifierId, DeclarationHandle> const& schemas,
                      bool fixed,
                      LoweringPathId prefix = {},
                      std::set<IdentifierId> ancestors = {}) -> FixedLayoutId;

auto build_fixed_layout(LoweringContext& context,
                        DeclarationHandle schema,
                        std::map<IdentifierId, DeclarationHandle> const& schemas,
                        LoweringPathId prefix = {},
                        std::set<IdentifierId> ancestors = {}) -> FixedLayoutId;
auto fixed_leaf_argument(LoweringContext& context, FixedLeafId leaf) -> std::string;
auto fixed_storage_node(LoweringContext& context, FixedLayoutId layout) -> NodeId;
auto fixed_container_node(LoweringContext& context,
                          FixedLayoutId layout,
                          std::string const& name,
                          CppTypeId equivalent_set_type = {}) -> NodeId;
} // namespace lispb::codegen::detail
