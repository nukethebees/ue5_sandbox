#pragma once

class UStaticMeshComponent;

namespace ml {
auto get_mesh_sphere_bounds(UStaticMeshComponent const& mesh) -> float;
}