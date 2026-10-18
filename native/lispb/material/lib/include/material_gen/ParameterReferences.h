#pragma once

#include <material_gen/Indices.h>
#include <material_gen/TextureSamplerType.h>

#include <vector>
namespace material_synth {

struct ParameterReferences {
    std::vector<ParameterId> indices;
    std::vector<TextureSamplerType> samplers;
};

}
