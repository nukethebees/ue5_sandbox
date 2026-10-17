#pragma once

#include "fixed_member_layout.h"
#include <codegen/schema/soa_schema.h>

#include <vector>

namespace codegen::detail {

struct FixedLayout {
    SoaSchema const* schema;
    std::vector<FixedMemberLayout> members;
    std::vector<FixedLeaf> leaves;
};

} // namespace codegen::detail
