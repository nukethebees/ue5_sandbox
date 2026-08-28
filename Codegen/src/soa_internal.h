#pragma once

#include "lowered_soa.h"
#include "resolved_member.h"

namespace codegen::detail {

auto resolve_members(SoaSchema const& schema,
                     std::map<std::string, CppType> const& types)
    -> std::vector<ResolvedMember>;
auto soa_equivalent_nodes(TypeRef const& equivalent_reference,
                          std::vector<ResolvedMember> const& members,
                          std::map<std::string, CppType> const& types) -> Nodes;
auto soa_view_specs(std::vector<ResolvedMember> const& members,
                    bool const_only) -> std::vector<FunctionSpec>;
auto soa_view_struct_nodes(SoaSchema const& schema,
                           std::vector<ResolvedMember> const& members,
                           std::map<std::string, CppType> const& types,
                           std::string const& view_name,
                           std::string const& const_view_name) -> Nodes;
auto soa_storage_view_nodes(std::vector<ResolvedMember> const& members) -> Nodes;
auto soa_storage_operation_specs(SoaSchema const& schema,
                                 std::vector<ResolvedMember> const& members)
    -> std::vector<FunctionSpec>;
auto soa_permutation_specs(std::vector<ResolvedMember> const& members)
    -> std::vector<FunctionSpec>;
auto soa_storage_node(SoaSchema const& schema,
                      std::vector<ResolvedMember> const& members,
                      std::string const& view_name,
                      std::string const& const_view_name,
                      std::map<std::string, CppType> const& types,
                      std::vector<FunctionSpec>& custom_source,
                      Nodes storage_prelude) -> Node;
auto lower_soa(SoaSchema const& schema,
               std::map<std::string, CppType> const& types,
               Nodes storage_prelude = {}) -> LoweredSoa;
auto lower_fixed_nodes(SoaSchema const& schema,
                       std::map<std::string, SoaSchema const*> const& schemas,
                       std::map<std::string, CppType> const& types) -> Nodes;

} // namespace codegen::detail
