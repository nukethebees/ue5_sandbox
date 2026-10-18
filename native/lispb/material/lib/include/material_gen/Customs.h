#pragma once

#include <lispb/indices.h>
#include <material_gen/NodeRange.h>

#include <vector>
namespace material_synth {

struct Customs {
    std::vector<lispb::StringId> descriptions;
    std::vector<lispb::StringId> code;
    std::vector<NodeRange> inputs;
};

}
