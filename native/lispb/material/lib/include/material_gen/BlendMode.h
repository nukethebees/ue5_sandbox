#pragma once

#include <cstdint>
namespace material_synth {

enum class BlendMode : std::uint8_t { additive, translucent, opaque, masked, alpha_composite };

}
