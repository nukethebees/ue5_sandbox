#include "SbxMeshGenLab/CylinderGenerator.h"

namespace SandboxMesh {
namespace {
void add_side(FSbxMeshData& mesh_data, FSbxCylinderParameters const& parameters) {
    auto const half_height{parameters.height * 0.5f};
    for (int32 segment_index{0}; segment_index <= parameters.radial_segments; ++segment_index) {
        auto const fraction{static_cast<float>(segment_index) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{UE_TWO_PI * fraction};
        auto const cosine{FMath::Cos(angle)};
        auto const sine{FMath::Sin(angle)};
        FVector3f const normal{cosine, sine, 0.0f};

        mesh_data.positions.Add(
            FVector3f{parameters.radius * cosine, parameters.radius * sine, -half_height});
        mesh_data.positions.Add(
            FVector3f{parameters.radius * cosine, parameters.radius * sine, half_height});
        mesh_data.normals.Append({normal, normal});
        mesh_data.uvs.Append({FVector2f{fraction, 1.0f}, FVector2f{fraction, 0.0f}});
    }

    for (int32 segment_index{0}; segment_index < parameters.radial_segments; ++segment_index) {
        auto const bottom{static_cast<uint32>(segment_index * 2)};
        auto const top{bottom + 1};
        auto const next_bottom{bottom + 2};
        auto const next_top{bottom + 3};
        mesh_data.indices.Append({bottom, next_top, next_bottom, bottom, top, next_top});
    }
}

void add_cap(FSbxMeshData& mesh_data,
             FSbxCylinderParameters const& parameters,
             float const z,
             FVector3f const normal) {
    auto const centre_index{static_cast<uint32>(mesh_data.positions.Num())};
    mesh_data.positions.Add(FVector3f{0.0f, 0.0f, z});
    mesh_data.normals.Add(normal);
    mesh_data.uvs.Add(FVector2f{0.5f, 0.5f});

    for (int32 segment_index{0}; segment_index < parameters.radial_segments; ++segment_index) {
        auto const fraction{static_cast<float>(segment_index) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{UE_TWO_PI * fraction};
        auto const cosine{FMath::Cos(angle)};
        auto const sine{FMath::Sin(angle)};
        mesh_data.positions.Add(FVector3f{parameters.radius * cosine, parameters.radius * sine, z});
        mesh_data.normals.Add(normal);
        mesh_data.uvs.Add(FVector2f{0.5f + cosine * 0.5f, 0.5f + sine * 0.5f});
    }

    for (int32 segment_index{0}; segment_index < parameters.radial_segments; ++segment_index) {
        auto const current{centre_index + 1 + static_cast<uint32>(segment_index)};
        auto const next{centre_index + 1 +
                        static_cast<uint32>((segment_index + 1) % parameters.radial_segments)};
        if (normal.Z > 0.0f) {
            mesh_data.indices.Append({centre_index, next, current});
        } else {
            mesh_data.indices.Append({centre_index, current, next});
        }
    }
}
}

auto generate_cylinder(FSbxCylinderParameters const& parameters) -> FSbxMeshData {
    check(parameters.radius > 0.0f);
    check(parameters.height > 0.0f);
    check(parameters.radial_segments >= 3);

    FSbxMeshData mesh_data{};
    auto const vertex_count{parameters.radial_segments * 4 + 4};
    auto const index_count{parameters.radial_segments * 12};
    mesh_data.positions.Reserve(vertex_count);
    mesh_data.normals.Reserve(vertex_count);
    mesh_data.uvs.Reserve(vertex_count);
    mesh_data.indices.Reserve(index_count);

    add_side(mesh_data, parameters);
    auto const half_height{parameters.height * 0.5f};
    add_cap(mesh_data, parameters, half_height, FVector3f::ZAxisVector);
    add_cap(mesh_data, parameters, -half_height, -FVector3f::ZAxisVector);

    return mesh_data;
}

}
