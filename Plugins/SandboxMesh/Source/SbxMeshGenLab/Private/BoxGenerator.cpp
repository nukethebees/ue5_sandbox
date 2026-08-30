#include "SbxMeshGenLab/BoxGenerator.h"

namespace SandboxMesh {
namespace {
void add_face(FSbxMeshData& mesh_data,
              FVector3f const normal,
              FVector3f const bottom_left,
              FVector3f const bottom_right,
              FVector3f const top_right,
              FVector3f const top_left) {
    auto const first_vertex{static_cast<uint32>(mesh_data.positions.Num())};

    mesh_data.positions.Append({bottom_left, bottom_right, top_right, top_left});
    mesh_data.normals.Append({normal, normal, normal, normal});
    mesh_data.uvs.Append({FVector2f{0.0f, 1.0f},
                          FVector2f{1.0f, 1.0f},
                          FVector2f{1.0f, 0.0f},
                          FVector2f{0.0f, 0.0f}});
    mesh_data.indices.Append({first_vertex,
                              first_vertex + 2,
                              first_vertex + 1,
                              first_vertex,
                              first_vertex + 3,
                              first_vertex + 2});
}
}

auto generate_box(FSbxBoxParameters const& parameters) -> FSbxMeshData {
    FSbxMeshData mesh_data{};
    mesh_data.positions.Reserve(24);
    mesh_data.normals.Reserve(24);
    mesh_data.uvs.Reserve(24);
    mesh_data.indices.Reserve(36);

    auto const half_dimensions{parameters.dimensions * 0.5f};
    auto const x{half_dimensions.X};
    auto const y{half_dimensions.Y};
    auto const z{half_dimensions.Z};

    add_face(mesh_data,
             FVector3f::XAxisVector,
             FVector3f{x, -y, -z},
             FVector3f{x, y, -z},
             FVector3f{x, y, z},
             FVector3f{x, -y, z});
    add_face(mesh_data,
             -FVector3f::XAxisVector,
             FVector3f{-x, y, -z},
             FVector3f{-x, -y, -z},
             FVector3f{-x, -y, z},
             FVector3f{-x, y, z});
    add_face(mesh_data,
             FVector3f::YAxisVector,
             FVector3f{-x, y, -z},
             FVector3f{-x, y, z},
             FVector3f{x, y, z},
             FVector3f{x, y, -z});
    add_face(mesh_data,
             -FVector3f::YAxisVector,
             FVector3f{x, -y, -z},
             FVector3f{x, -y, z},
             FVector3f{-x, -y, z},
             FVector3f{-x, -y, -z});
    add_face(mesh_data,
             FVector3f::ZAxisVector,
             FVector3f{-x, -y, z},
             FVector3f{x, -y, z},
             FVector3f{x, y, z},
             FVector3f{-x, y, z});
    add_face(mesh_data,
             -FVector3f::ZAxisVector,
             FVector3f{-x, y, -z},
             FVector3f{x, y, -z},
             FVector3f{x, -y, -z},
             FVector3f{-x, -y, -z});

    return mesh_data;
}

}
