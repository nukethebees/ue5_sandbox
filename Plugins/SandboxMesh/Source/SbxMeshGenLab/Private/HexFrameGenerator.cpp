#include "SbxMeshGenLab/HexFrameGenerator.h"

#include "Containers/StaticArray.h"

namespace SandboxMesh {
namespace {
constexpr int32 side_count{6};

void add_quad(FSbxMeshData& mesh_data,
              FVector3f const first,
              FVector3f const second,
              FVector3f const third,
              FVector3f const fourth,
              FVector3f const normal) {
    auto const base_index{static_cast<uint32>(mesh_data.positions.Num())};
    mesh_data.positions.Append({first, second, third, fourth});
    mesh_data.normals.Append({normal, normal, normal, normal});
    mesh_data.uvs.Append({FVector2f{0.0f, 1.0f},
                          FVector2f{1.0f, 1.0f},
                          FVector2f{1.0f, 0.0f},
                          FVector2f{0.0f, 0.0f}});
    mesh_data.indices.Append(
        {base_index, base_index + 2, base_index + 1, base_index, base_index + 3, base_index + 2});
}

auto make_ring(float const radius, float const z, float const angle_offset)
    -> TStaticArray<FVector3f, side_count> {
    TStaticArray<FVector3f, side_count> positions{};
    for (int32 side_index{0}; side_index < side_count; ++side_index) {
        auto const angle{angle_offset + UE_TWO_PI * static_cast<float>(side_index) /
                                            static_cast<float>(side_count)};
        positions[side_index] =
            FVector3f{radius * FMath::Cos(angle), radius * FMath::Sin(angle), z};
    }
    return positions;
}
}

auto generate_hex_frame(FSbxHexFrameParameters const& parameters) -> FSbxMeshData {
    check(parameters.outer_radius > 0.0f);
    check(parameters.wall_thickness > 0.0f);
    check(parameters.wall_thickness < parameters.outer_radius);
    check(parameters.depth > 0.0f);

    FSbxMeshData mesh_data{};
    constexpr int32 surface_count{4};
    constexpr int32 vertices_per_quad{4};
    constexpr int32 indices_per_quad{6};
    mesh_data.positions.Reserve(side_count * surface_count * vertices_per_quad);
    mesh_data.normals.Reserve(side_count * surface_count * vertices_per_quad);
    mesh_data.uvs.Reserve(side_count * surface_count * vertices_per_quad);
    mesh_data.indices.Reserve(side_count * surface_count * indices_per_quad);

    auto const inner_radius{parameters.outer_radius - parameters.wall_thickness};
    auto const half_depth{parameters.depth * 0.5f};
    auto const angle_offset{parameters.pointy_top ? UE_PI * 0.5f : 0.0f};
    auto const outer_front{make_ring(parameters.outer_radius, half_depth, angle_offset)};
    auto const outer_back{make_ring(parameters.outer_radius, -half_depth, angle_offset)};
    auto const inner_front{make_ring(inner_radius, half_depth, angle_offset)};
    auto const inner_back{make_ring(inner_radius, -half_depth, angle_offset)};

    for (int32 side_index{0}; side_index < side_count; ++side_index) {
        auto const next_index{(side_index + 1) % side_count};
        auto const face_normal{
            (outer_front[side_index] + outer_front[next_index]).GetSafeNormal2D()};

        add_quad(mesh_data,
                 outer_front[side_index],
                 outer_front[next_index],
                 inner_front[next_index],
                 inner_front[side_index],
                 FVector3f::ZAxisVector);
        add_quad(mesh_data,
                 outer_back[side_index],
                 inner_back[side_index],
                 inner_back[next_index],
                 outer_back[next_index],
                 -FVector3f::ZAxisVector);
        add_quad(mesh_data,
                 outer_back[side_index],
                 outer_back[next_index],
                 outer_front[next_index],
                 outer_front[side_index],
                 face_normal);
        add_quad(mesh_data,
                 inner_back[side_index],
                 inner_front[side_index],
                 inner_front[next_index],
                 inner_back[next_index],
                 -face_normal);
    }

    return mesh_data;
}

}
