#pragma once

#include <Components/InstancedStaticMeshComponent.h>
#include <Misc/Optional.h>

namespace ml::batch {
struct FCullDistances {
    float min_distance{0.0f};
    float max_distance{0.0f};
};

struct FIsmcConfig {
    UStaticMesh* mesh{nullptr};
    UMaterialInterface* material{nullptr};
    TOptional<FCullDistances> cull_distances{NullOpt};
    TOptional<int32> num_custom_data_floats{NullOpt};
};

SPACEGAMEPRESENTATION_API void configure_ismc(UInstancedStaticMeshComponent& instances,
                                              FIsmcConfig const& config);

}
