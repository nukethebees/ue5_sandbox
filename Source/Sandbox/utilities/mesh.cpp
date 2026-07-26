#include "mesh.h"

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Engine/StaticMesh.h>

namespace ml {
auto get_mesh_sphere_bounds(UStaticMeshComponent const& mesh) -> float {
    return static_cast<float>(mesh.GetStaticMesh()->GetBounds().SphereRadius);
}
}
