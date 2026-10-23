#pragma once

#include "lowered_soa.h"
#include "lowering_context.h"
#include "soa_functions.h"

#include <codegen/ast/indices.h>
#include <codegen/ast/node_id.h>
#include <codegen/ast/nodes.h>
#include <codegen/lowering/indices.h>
#include <codegen/schema/indices.h>
#include <codegen/schema/soa_backend.h>
#include <codegen/schema/soa_schemas.h>
#include <lispb/indices.h>
#include <lispb/schema/type_graph.h>

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace lispb::codegen::detail {

auto resolve_members(LoweringContext& context, DeclarationHandle schema)
    -> std::vector<ResolvedMemberId>;
auto soa_equivalent_nodes(LoweringContext& context,
                          TypeRefId equivalent_reference,
                          std::span<ResolvedMemberId const> members) -> Nodes;
auto soa_view_functions(LoweringContext& context,
                        std::span<ResolvedMemberId const> members,
                        bool const_only) -> std::vector<FunctionId>;
auto soa_view_struct_nodes(LoweringContext& context,
                           DeclarationHandle schema,
                           std::span<ResolvedMemberId const> members,
                           std::string const& view_name,
                           std::string const& const_view_name,
                           SoaFunctions const& functions,
                           LoweredSoa const& const_api,
                           LoweredSoa const& mutable_api,
                           std::map<StringId, DeclarationHandle> const* schemas = nullptr) -> Nodes;
auto soa_storage_view_nodes(LoweringContext& context,
                            std::span<ResolvedMemberId const> members,
                            std::span<FunctionId const> functions) -> Nodes;
auto soa_storage_functions(LoweringContext& context,
                           DeclarationHandle schema,
                           std::span<ResolvedMemberId const> members) -> std::vector<FunctionId>;
auto soa_set_function(LoweringContext& context,
                      DeclarationHandle schema,
                      std::span<ResolvedMemberId const> members,
                      bool is_const) -> std::optional<FunctionId>;
auto soa_add_function(LoweringContext& context,
                      DeclarationHandle schema,
                      std::span<ResolvedMemberId const> members) -> std::optional<FunctionId>;
auto soa_permutation_functions(LoweringContext& context, std::span<ResolvedMemberId const> members)
    -> std::vector<FunctionId>;
auto soa_storage_node(LoweringContext& context,
                      DeclarationHandle schema,
                      std::span<ResolvedMemberId const> members,
                      std::string const& view_name,
                      std::string const& const_view_name,
                      SoaFunctions const& functions,
                      Nodes& custom_source,
                      Nodes storage_prelude,
                      std::map<StringId, DeclarationHandle> const* schemas = nullptr) -> NodeId;
auto lower_soa(LoweringContext& context,
               DeclarationHandle schema,
               Nodes storage_prelude = {},
               Nodes mutable_view_prelude = {},
               Nodes view_prelude = {}) -> LoweredSoa;
auto lower_fixed_nodes(LoweringContext& context,
                       DeclarationHandle schema,
                       std::map<StringId, DeclarationHandle> const& schemas,
                       CppTypeId equivalent_set_type = {}) -> Nodes;

auto lower_single_allocation(LoweringContext& context,
                             DeclarationHandle schema,
                             std::map<StringId, DeclarationHandle> const& schemas,
                             SoaBackend backend) -> LoweredSoa;

auto lower_native_soa(LoweringContext& context,
                      DeclarationHandle schema,
                      std::map<StringId, DeclarationHandle> const& schemas,
                      bool allow_equivalent_type = false,
                      std::span<std::string const> equivalent_members = {},
                      StringId equivalent_constructor = {},
                      bool equivalent_accessors = false) -> LoweredSoa;

} // namespace lispb::codegen::detail
