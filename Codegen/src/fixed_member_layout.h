#pragma once

#include "fixed_leaf.h"
#include "resolved_member.h"

#include <optional>
#include <string>
#include <vector>

namespace codegen::detail {

struct FixedMemberLayout {
    SoaMemberSchema const* schema;
    ResolvedMember member;
    std::vector<FixedLeaf> leaves;
    std::optional<std::string> nested_storage_name;
};

} // namespace codegen::detail
