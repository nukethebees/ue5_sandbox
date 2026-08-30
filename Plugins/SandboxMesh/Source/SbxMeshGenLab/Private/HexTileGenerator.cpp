#include "SbxMeshGenLab/HexTileGenerator.h"

#include "Containers/StaticArray.h"

namespace SandboxMesh {
namespace {
constexpr int32 hex_tile_side_count{6};

void add_hex_tile_triangle(FSbxMeshData& mesh_data,
                           FVector3f const first,
                           FVector3f const second,
                           FVector3f const third,
                           FVector3f const normal,
                           float const uv_radius) {
    auto const make_uv = [uv_radius](FVector3f const position) {
        return FVector2f{position.X / (2.0f * uv_radius) + 0.5f,
                         position.Y / (2.0f * uv_radius) + 0.5f};
    };
    auto const base_index{static_cast<uint32>(mesh_data.positions.Num())};
    mesh_data.positions.Append({first, second, third});
    mesh_data.normals.Append({normal, normal, normal});
    mesh_data.uvs.Append({make_uv(first), make_uv(second), make_uv(third)});
    mesh_data.indices.Append({base_index, base_index + 2, base_index + 1});
}

void add_hex_tile_quad(FSbxMeshData& mesh_data,
                       FVector3f const first,
                       FVector3f const second,
                       FVector3f const third,
                       FVector3f const fourth) {
    auto const normal{FVector3f::CrossProduct(second - first, third - first).GetSafeNormal()};
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

auto make_hex_tile_ring(float const radius, float const z, float const angle_offset)
    -> TStaticArray<FVector3f, hex_tile_side_count> {
    TStaticArray<FVector3f, hex_tile_side_count> positions{};
    for (int32 side_index{0}; side_index < hex_tile_side_count; ++side_index) {
        auto const angle{angle_offset + UE_TWO_PI * static_cast<float>(side_index) /
                                            static_cast<float>(hex_tile_side_count)};
        positions[side_index] =
            FVector3f{radius * FMath::Cos(angle), radius * FMath::Sin(angle), z};
    }
    return positions;
}
}

auto generate_hex_tile(FSbxHexTileParameters const& parameters) -> FSbxMeshData {
    check(parameters.outer_radius > 0.0f);
    check(parameters.depth > 0.0f);
    check(parameters.bevel_width > 0.0f);
    check(parameters.bevel_width < parameters.outer_radius);
    check(parameters.bevel_width < parameters.depth * 0.5f);

    constexpr int32 face_triangle_count{hex_tile_side_count * 2};
    constexpr int32 surface_quad_count{hex_tile_side_count * 3};
    constexpr int32 vertices_per_triangle{3};
    constexpr int32 vertices_per_quad{4};
    constexpr int32 indices_per_triangle{3};
    constexpr int32 indices_per_quad{6};
    FSbxMeshData mesh_data{};
    mesh_data.positions.Reserve(face_triangle_count * vertices_per_triangle +
                                surface_quad_count * vertices_per_quad);
    mesh_data.normals.Reserve(mesh_data.positions.Max());
    mesh_data.uvs.Reserve(mesh_data.positions.Max());
    mesh_data.indices.Reserve(face_triangle_count * indices_per_triangle +
                              surface_quad_count * indices_per_quad);

    auto const half_depth{parameters.depth * 0.5f};
    auto const face_radius{parameters.outer_radius - parameters.bevel_width};
    auto const wall_half_depth{half_depth - parameters.bevel_width};
    auto const angle_offset{parameters.pointy_top ? UE_PI * 0.5f : 0.0f};
    auto const top_face{make_hex_tile_ring(face_radius, half_depth, angle_offset)};
    auto const top_wall{make_hex_tile_ring(parameters.outer_radius, wall_half_depth, angle_offset)};
    auto const bottom_wall{
        make_hex_tile_ring(parameters.outer_radius, -wall_half_depth, angle_offset)};
    auto const bottom_face{make_hex_tile_ring(face_radius, -half_depth, angle_offset)};
    auto const top_center{FVector3f{0.0f, 0.0f, half_depth}};
    auto const bottom_center{FVector3f{0.0f, 0.0f, -half_depth}};

    for (int32 side_index{0}; side_index < hex_tile_side_count; ++side_index) {
        auto const next_index{(side_index + 1) % hex_tile_side_count};

        add_hex_tile_triangle(mesh_data,
                              top_center,
                              top_face[side_index],
                              top_face[next_index],
                              FVector3f::ZAxisVector,
                              parameters.outer_radius);
        add_hex_tile_triangle(mesh_data,
                              bottom_center,
                              bottom_face[next_index],
                              bottom_face[side_index],
                              -FVector3f::ZAxisVector,
                              parameters.outer_radius);
        add_hex_tile_quad(mesh_data,
                          top_face[side_index],
                          top_wall[side_index],
                          top_wall[next_index],
                          top_face[next_index]);
        add_hex_tile_quad(mesh_data,
                          top_wall[side_index],
                          bottom_wall[side_index],
                          bottom_wall[next_index],
                          top_wall[next_index]);
        add_hex_tile_quad(mesh_data,
                          bottom_wall[side_index],
                          bottom_face[side_index],
                          bottom_face[next_index],
                          bottom_wall[next_index]);
    }

    return mesh_data;
}

}
