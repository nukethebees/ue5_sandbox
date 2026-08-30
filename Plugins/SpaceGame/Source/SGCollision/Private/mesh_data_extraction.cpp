#include "SGCollision/mesh_data_extraction.h"

#include <Engine/StaticMesh.h>
#include <Math/Transform.h>
#include <PhysicsEngine/BodySetup.h>

namespace ml {
auto get_aabb(UStaticMesh const& mesh) -> FBox {
    auto const* body_setup{mesh.GetBodySetup()};
    if (body_setup == nullptr) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Cannot extract collision AABB from static mesh %s: mesh has no body setup"),
               *mesh.GetName());
        return FBox{ForceInit};
    }

    FBox aabb{ForceInit};

    for (auto const& collision_box : body_setup->AggGeom.BoxElems) {
        aabb += collision_box.CalcAABB(FTransform::Identity, 1.0f);
    }

    return aabb;
}
}
