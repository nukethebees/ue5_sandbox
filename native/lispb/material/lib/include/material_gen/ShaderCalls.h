#pragma once

#include <lispb/indices.h>
#include <material_gen/NodeRange.h>

#include <vector>
namespace material_synth {

struct ShaderCalls {
    std::vector<lispb::StringId> paths;
    std::vector<lispb::StringId> functions;
    std::vector<NodeRange> inputs;
};

}
