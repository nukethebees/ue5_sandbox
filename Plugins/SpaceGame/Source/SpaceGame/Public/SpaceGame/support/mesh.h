#pragma once

class UStaticMeshComponent;
class UStaticMesh;
class UPrimitiveComponent;

namespace ml {
auto get_static_mesh(UPrimitiveComponent const* component) -> UStaticMesh const*;
auto get_mesh_sphere_bounds(UStaticMesh const& mesh) -> float;
auto get_mesh_sphere_bounds(UStaticMeshComponent const& mesh) -> float;
}
