#pragma once

#include <array>
#include <cstdint>
#include <vector>
namespace material_synth {

struct Constants {
    std::array<std::vector<double>, 4> components;
    std::vector<std::uint8_t> counts;
};

}
