#include "SbxMeshGenLab/SphereGenerator.h"

namespace SandboxMesh {

auto generate_sphere(FSbxSphereParameters const& parameters) -> FSbxMeshData {
    check(parameters.radius > 0.0f);
    check(parameters.longitude_segments >= 3);
    check(parameters.latitude_segments >= 2);

    FSbxMeshData mesh_data{};
    auto const vertices_per_row{parameters.longitude_segments + 1};
    auto const vertex_count{(parameters.latitude_segments + 1) * vertices_per_row};
    auto const index_count{parameters.longitude_segments * (parameters.latitude_segments - 1) * 6};
    mesh_data.positions.Reserve(vertex_count);
    mesh_data.normals.Reserve(vertex_count);
    mesh_data.uvs.Reserve(vertex_count);
    mesh_data.indices.Reserve(index_count);

    for (int32 latitude_index{0}; latitude_index <= parameters.latitude_segments;
         ++latitude_index) {
        auto const v{static_cast<float>(latitude_index) /
                     static_cast<float>(parameters.latitude_segments)};
        auto const latitude_angle{UE_PI * v};
        auto const radial_distance{FMath::Sin(latitude_angle)};
        auto const z{FMath::Cos(latitude_angle)};

        for (int32 longitude_index{0}; longitude_index <= parameters.longitude_segments;
             ++longitude_index) {
            auto const u{static_cast<float>(longitude_index) /
                         static_cast<float>(parameters.longitude_segments)};
            auto const longitude_angle{UE_TWO_PI * u};
            FVector3f const direction{radial_distance * FMath::Cos(longitude_angle),
                                      radial_distance * FMath::Sin(longitude_angle),
                                      z};
            auto const normal{direction.GetSafeNormal()};

            mesh_data.positions.Add(normal * parameters.radius);
            mesh_data.normals.Add(normal);
            mesh_data.uvs.Add(FVector2f{u, v});
        }
    }

    for (int32 latitude_index{0}; latitude_index < parameters.latitude_segments; ++latitude_index) {
        for (int32 longitude_index{0}; longitude_index < parameters.longitude_segments;
             ++longitude_index) {
            auto const upper_left{
                static_cast<uint32>(latitude_index * vertices_per_row + longitude_index)};
            auto const upper_right{upper_left + 1};
            auto const lower_left{upper_left + static_cast<uint32>(vertices_per_row)};
            auto const lower_right{lower_left + 1};

            if (latitude_index == 0) {
                mesh_data.indices.Append({upper_left, lower_right, lower_left});
            } else if (latitude_index == parameters.latitude_segments - 1) {
                mesh_data.indices.Append({upper_left, upper_right, lower_left});
            } else {
                mesh_data.indices.Append(
                    {upper_left, lower_right, lower_left, upper_left, upper_right, lower_right});
            }
        }
    }

    return mesh_data;
}

}
