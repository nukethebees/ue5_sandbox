#pragma once

#include <lispb/indices.h>
#include <slate_codegen/child_id.h>

#include <vector>
namespace slate_codegen::detail {

struct NamedSlots {
    std::vector<lispb::IdentifierId> names;
    std::vector<ChildId> children;
};

}
