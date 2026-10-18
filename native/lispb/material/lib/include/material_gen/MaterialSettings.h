#pragma once

#include <lispb/indices.h>
#include <material_gen/BlendMode.h>
#include <material_gen/MaterialDomain.h>
#include <material_gen/ShadingModel.h>

namespace material_synth {

struct MaterialSettings {
    lispb::StringId name;
    lispb::StringId package_path;
    MaterialDomain domain{MaterialDomain::ui};
    BlendMode blend_mode{BlendMode::additive};
    ShadingModel shading_model{ShadingModel::default_lit};
    bool two_sided{};
    bool disable_depth_test{};
    bool used_with_instanced_static_meshes{};
    bool used_with_particle_sprites{};
    bool adopt_existing{};
    double opacity_mask_clip_value{0.3333};
};

}
