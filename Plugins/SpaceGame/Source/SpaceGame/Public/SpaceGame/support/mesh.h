#pragma once

class UStaticMeshComponent;
class UStaticMesh;

namespace ml {
auto get_mesh_sphere_bounds(UStaticMesh const& mesh) -> float;
auto get_mesh_sphere_bounds(UStaticMeshComponent const& mesh) -> float;
}
