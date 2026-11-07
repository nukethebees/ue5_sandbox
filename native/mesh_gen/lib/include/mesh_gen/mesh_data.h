#pragma once

#include <mesh_gen/material_role.h>
#include <mesh_gen/vector_types.h>

#include <cstdint>
#include <vector>

namespace mesh_gen {

struct MeshData {
    std::vector<Vec3f> positions;
    std::vector<Vec3f> normals;
    std::vector<Vec2f> uvs;
    std::vector<std::uint32_t> indices;
    std::vector<MaterialRole> triangle_material_roles;
};

[[nodiscard]] auto is_valid_mesh_data(MeshData const& mesh_data) -> bool;

}
