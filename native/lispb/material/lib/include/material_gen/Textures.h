#pragma once

#include <lispb/indices.h>
#include <material_gen/TextureSamplerType.h>

#include <vector>
namespace material_synth {

struct Textures {
    std::vector<lispb::StringId> paths;
    std::vector<TextureSamplerType> samplers;
};

}
