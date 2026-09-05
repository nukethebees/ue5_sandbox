#pragma once

#include <CoreMinimal.h>

class UStaticMeshComponent;
class UStaticMesh;
class UPrimitiveComponent;

namespace ml {
auto get_static_mesh(UPrimitiveComponent const* component) -> UStaticMesh const*;
SPACEGAME_API float get_mesh_sphere_bounds(UStaticMesh const& mesh);
SPACEGAME_API float get_mesh_sphere_bounds(UStaticMeshComponent const& mesh);
}
