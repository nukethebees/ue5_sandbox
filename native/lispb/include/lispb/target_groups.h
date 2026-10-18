#pragma once

#include <codegen/sexpr/handle_range.h>
#include <lispb/indices.h>
#include <lispb/target_id.h>

#include <vector>
namespace lispb {

struct TargetGroups {
    std::vector<IdentifierId> names;
    std::vector<codegen::sexpr::HandleRange> members;
    std::vector<TargetId> targets;
};

}
