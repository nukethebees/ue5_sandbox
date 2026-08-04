#include "mesh.h"

#include <Components/InstancedStaticMeshComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Engine/StaticMesh.h>

namespace ml {
auto get_mesh_sphere_bounds(UStaticMesh const& mesh) -> float {
    return static_cast<float>(mesh.GetBounds().SphereRadius);
}
auto get_mesh_sphere_bounds(UStaticMeshComponent const& mesh) -> float {
    auto const static_mesh{mesh.GetStaticMesh()};
    check(static_mesh);
    return get_mesh_sphere_bounds(*static_mesh);
}
}
