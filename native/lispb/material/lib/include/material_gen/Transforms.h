#pragma once

#include <material_gen/PositionSpace.h>

#include <vector>
namespace material_synth {

struct Transforms {
    std::vector<PositionSpace> sources;
    std::vector<PositionSpace> destinations;
};

}
