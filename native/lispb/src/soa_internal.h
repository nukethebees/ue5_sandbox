#pragma once

#include "lowered_soa.h"
#include "resolved_member.h"

#include <lispb/schema/type_graph.h>

#include <span>
#include <string>
#include <string_view>

namespace codegen::detail {

auto resolve_members(SoaSchema const& schema, TypeRegistry const& types)
    -> std::vector<ResolvedMember>;
auto soa_function_spec(FunctionSchema const& schema, TypeRegistry const& types) -> FunctionSpec;
auto soa_equivalent_nodes(TypeRef const& equivalent_reference,
                          std::vector<ResolvedMember> const& members,
                          TypeRegistry const& types) -> Nodes;
auto soa_view_specs(std::vector<ResolvedMember> const& members, bool const_only)
    -> std::vector<FunctionSpec>;
auto soa_view_struct_nodes(SoaSchema const& schema,
                           std::vector<ResolvedMember> const& members,
                           TypeRegistry const& types,
                           std::string const& view_name,
                           std::string const& const_view_name) -> Nodes;
auto soa_storage_view_nodes(std::vector<ResolvedMember> const& members) -> Nodes;
auto soa_storage_operation_specs(SoaSchema const& schema,
                                 std::vector<ResolvedMember> const& members)
    -> std::vector<FunctionSpec>;
auto soa_set_spec(SoaSchema const& schema,
                  std::vector<ResolvedMember> const& members,
                  bool is_const) -> std::optional<FunctionSpec>;
auto soa_add_spec(SoaSchema const& schema, std::vector<ResolvedMember> const& members)
    -> std::optional<FunctionSpec>;
auto soa_permutation_specs(std::vector<ResolvedMember> const& members) -> std::vector<FunctionSpec>;
auto soa_storage_node(SoaSchema const& schema,
                      std::vector<ResolvedMember> const& members,
                      std::string const& view_name,
                      std::string const& const_view_name,
                      TypeRegistry const& types,
                      std::vector<FunctionSpec>& custom_source,
                      Nodes storage_prelude) -> Node;
auto lower_soa(SoaSchema const& schema, TypeRegistry const& types, Nodes storage_prelude = {})
    -> LoweredSoa;
auto lower_fixed_nodes(SoaSchema const& schema,
                       std::map<std::string, SoaSchema const*> const& schemas,
                       TypeRegistry const& types) -> Nodes;

auto lower_single_allocation_nodes(SoaSchema const& schema,
                                   std::map<std::string, SoaSchema const*> const& schemas,
                                   TypeRegistry const& types,
                                   lispb::schema::TypeGraph const& type_graph,
                                   std::string const& module_name,
                                   SoaBackend backend) -> Nodes;

auto lower_native_soa(SoaSchema const& schema,
                      std::map<std::string, SoaSchema const*> const& schemas,
                      TypeRegistry const& types,
                      bool allow_equivalent_type = false,
                      std::span<std::string const> equivalent_members = {},
                      std::string_view equivalent_constructor = {}) -> LoweredSoa;

} // namespace codegen::detail
