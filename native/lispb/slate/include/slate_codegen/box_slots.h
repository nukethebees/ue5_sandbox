#pragma once

#include <slate_codegen/alignment.h>
#include <slate_codegen/child_id.h>
#include <slate_codegen/value_id.h>

#include <cstdint>
#include <vector>
namespace slate_codegen::detail {

struct BoxSlots {
    std::vector<std::uint8_t> fill;
    std::vector<ValueId> weights;
    std::vector<ValueId> padding;
    std::vector<Alignment> horizontal;
    std::vector<Alignment> vertical;
    std::vector<ChildId> children;
};

}
