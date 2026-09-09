#include "SpaceGamePresentation/entities/TestBatchActorCore.h"

namespace ml::batch {
void configure_ismc(UInstancedStaticMeshComponent& instances, FIsmcConfig const& config) {
    instances.SetMobility(EComponentMobility::Movable);
    instances.SetStaticMesh(config.mesh);
    check(instances.GetStaticMesh() == config.mesh);
    instances.SetMobility(EComponentMobility::Static);

    if (IsValid(config.material)) {
        instances.SetMaterial(0, config.material);
    }

    instances.SetCanEverAffectNavigation(false);
    instances.SetCollisionEnabled(ECollisionEnabled::NoCollision);
    instances.SetGenerateOverlapEvents(false);
    instances.SetCastShadow(false);
    instances.SetAffectDistanceFieldLighting(false);
    instances.SetReceivesDecals(false);
    instances.SetRemoveSwap();

    if (config.cull_distances.IsSet()) {
        auto const& cull_distances{config.cull_distances.GetValue()};
        instances.SetCullDistances(cull_distances.min_distance, cull_distances.max_distance);
    }

    if (config.num_custom_data_floats.IsSet()) {
        instances.SetNumCustomDataFloats(config.num_custom_data_floats.GetValue());
    }
}

}
