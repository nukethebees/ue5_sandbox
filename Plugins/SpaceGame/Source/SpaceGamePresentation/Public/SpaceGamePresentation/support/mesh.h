#pragma once

#include <CoreMinimal.h>

class UStaticMeshComponent;
class UStaticMesh;
class UPrimitiveComponent;

namespace ml {
auto get_static_mesh(UPrimitiveComponent const* component) -> UStaticMesh const*;
SPACEGAMEPRESENTATION_API float get_mesh_sphere_bounds(UStaticMesh const& mesh);
SPACEGAMEPRESENTATION_API float get_mesh_sphere_bounds(UStaticMeshComponent const& mesh);
}
