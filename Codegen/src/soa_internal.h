#pragma once

#include "lowered_soa.h"
#include "resolved_member.h"

namespace codegen::detail {

auto resolve_members(SoaSchema const& schema,
                     std::map<std::string, CppType> const& types)
    -> std::vector<ResolvedMember>;
auto lower_soa(SoaSchema const& schema,
               std::map<std::string, CppType> const& types) -> LoweredSoa;
auto lower_fixed_nodes(SoaSchema const& schema,
                       std::map<std::string, SoaSchema const*> const& schemas,
                       std::map<std::string, CppType> const& types) -> Nodes;

} // namespace codegen::detail
