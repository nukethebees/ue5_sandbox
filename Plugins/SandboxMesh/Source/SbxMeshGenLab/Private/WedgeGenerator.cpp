#include "SbxMeshGenLab/WedgeGenerator.h"

#include "Algo/Reverse.h"

namespace SandboxMesh {
namespace {

void add_polygon(FSbxMeshData& mesh_data, TArray<FVector3f> positions) {
    check(positions.Num() >= 3);
    auto normal{FVector3f::CrossProduct(positions[1] - positions[0], positions[2] - positions[0])
                    .GetSafeNormal()};
    FVector3f center{FVector3f::ZeroVector};
    for (auto const position : positions) {
        center += position;
    }
    center /= static_cast<float>(positions.Num());
    if (FVector3f::DotProduct(normal, center) < 0.0f) {
        Algo::Reverse(positions);
        normal *= -1.0f;
    }

    auto const tangent_reference{FMath::Abs(normal.Z) < 0.9f ? FVector3f::ZAxisVector
                                                             : FVector3f::YAxisVector};
    auto const tangent{FVector3f::CrossProduct(tangent_reference, normal).GetSafeNormal()};
    auto const bitangent{FVector3f::CrossProduct(normal, tangent)};
    FVector2f uv_min{MAX_flt, MAX_flt};
    FVector2f uv_max{-MAX_flt, -MAX_flt};
    TArray<FVector2f> projected_uvs;
    projected_uvs.Reserve(positions.Num());
    for (auto const position : positions) {
        auto const uv{FVector2f{FVector3f::DotProduct(position, tangent),
                                FVector3f::DotProduct(position, bitangent)}};
        projected_uvs.Add(uv);
        uv_min.X = FMath::Min(uv_min.X, uv.X);
        uv_min.Y = FMath::Min(uv_min.Y, uv.Y);
        uv_max.X = FMath::Max(uv_max.X, uv.X);
        uv_max.Y = FMath::Max(uv_max.Y, uv.Y);
    }

    auto const uv_size{uv_max - uv_min};
    auto const base_index{static_cast<uint32>(mesh_data.positions.Num())};
    auto const vertex_count{positions.Num()};
    for (int32 vertex_index{}; vertex_index < vertex_count; ++vertex_index) {
        auto const uv{projected_uvs[vertex_index] - uv_min};
        mesh_data.positions.Add(positions[vertex_index]);
        mesh_data.normals.Add(normal);
        mesh_data.uvs.Add(FVector2f{uv_size.X > UE_SMALL_NUMBER ? uv.X / uv_size.X : 0.0f,
                                    uv_size.Y > UE_SMALL_NUMBER ? uv.Y / uv_size.Y : 0.0f});
    }
    for (int32 vertex_index{1}; vertex_index < vertex_count - 1; ++vertex_index) {
        mesh_data.indices.Append({base_index,
                                  base_index + static_cast<uint32>(vertex_index + 1),
                                  base_index + static_cast<uint32>(vertex_index)});
    }
}

}

auto generate_wedge(FSbxWedgeParameters const& parameters) -> FSbxMeshData {
    check(parameters.dimensions.GetMin() > 0.0f);
    check(parameters.top_length > 0.0f);
    check(parameters.top_length <= parameters.dimensions.X);
    check(FMath::Abs(parameters.top_offset) + parameters.top_length * 0.5f <=
          parameters.dimensions.X * 0.5f);

    FSbxMeshData mesh_data;
    mesh_data.positions.Reserve(24);
    mesh_data.normals.Reserve(24);
    mesh_data.uvs.Reserve(24);
    mesh_data.indices.Reserve(36);

    auto const half_dimensions{parameters.dimensions * 0.5f};
    auto const top_half_length{parameters.top_length * 0.5f};
    auto const bottom_left_x{-half_dimensions.X};
    auto const bottom_right_x{half_dimensions.X};
    auto const top_left_x{parameters.top_offset - top_half_length};
    auto const top_right_x{parameters.top_offset + top_half_length};
    auto const front_y{-half_dimensions.Y};
    auto const back_y{half_dimensions.Y};
    auto const bottom_z{-half_dimensions.Z};
    auto const top_z{half_dimensions.Z};

    FVector3f const front_bottom_left{bottom_left_x, front_y, bottom_z};
    FVector3f const front_bottom_right{bottom_right_x, front_y, bottom_z};
    FVector3f const front_top_left{top_left_x, front_y, top_z};
    FVector3f const front_top_right{top_right_x, front_y, top_z};
    FVector3f const back_bottom_left{bottom_left_x, back_y, bottom_z};
    FVector3f const back_bottom_right{bottom_right_x, back_y, bottom_z};
    FVector3f const back_top_left{top_left_x, back_y, top_z};
    FVector3f const back_top_right{top_right_x, back_y, top_z};

    add_polygon(mesh_data,
                {front_bottom_left, front_top_left, front_top_right, front_bottom_right});
    add_polygon(mesh_data, {back_bottom_left, back_bottom_right, back_top_right, back_top_left});
    add_polygon(mesh_data,
                {front_bottom_left, front_bottom_right, back_bottom_right, back_bottom_left});
    add_polygon(mesh_data, {front_top_left, back_top_left, back_top_right, front_top_right});
    add_polygon(mesh_data, {front_bottom_left, back_bottom_left, back_top_left, front_top_left});
    add_polygon(mesh_data,
                {front_bottom_right, front_top_right, back_top_right, back_bottom_right});

    return mesh_data;
}

}
