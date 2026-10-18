#pragma once

#include <material_gen/TextureResolveFunction.h>

namespace material_synth {

struct TextureResolver {
    void const* context{};
    TextureResolveFunction resolve{};
};

}
