#include "SbxMeshGenLab/BeveledBoxGenerator.h"

#include "Algo/Reverse.h"

namespace SandboxMesh {
namespace {

void add_polygon(FSbxMeshData& mesh_data, FVector3f const normal, TArray<FVector3f> positions) {
    check(positions.Num() >= 3);
    if (FVector3f::DotProduct(
            FVector3f::CrossProduct(positions[1] - positions[0], positions[2] - positions[0]),
            normal) < 0.0f) {
        Algo::Reverse(positions);
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

void add_main_face(FSbxMeshData& mesh_data,
                   FVector3f const center,
                   FVector3f const normal,
                   FVector3f const horizontal,
                   FVector3f const vertical,
                   float const half_width,
                   float const half_height,
                   float const bevel_width) {
    TArray<FVector2f> const outline{{-half_width + bevel_width, -half_height},
                                    {half_width - bevel_width, -half_height},
                                    {half_width, -half_height + bevel_width},
                                    {half_width, half_height - bevel_width},
                                    {half_width - bevel_width, half_height},
                                    {-half_width + bevel_width, half_height},
                                    {-half_width, half_height - bevel_width},
                                    {-half_width, -half_height + bevel_width}};
    TArray<FVector3f> positions;
    positions.Reserve(outline.Num());
    for (auto const point : outline) {
        positions.Add(center + horizontal * point.X + vertical * point.Y);
    }
    add_polygon(mesh_data, normal, MoveTemp(positions));
}

}

auto generate_beveled_box(FSbxBeveledBoxParameters const& parameters) -> FSbxMeshData {
    check(parameters.dimensions.GetMin() > 0.0f);
    check(parameters.bevel_width > 0.0f);
    check(parameters.bevel_width < parameters.dimensions.GetMin() * 0.5f);

    FSbxMeshData mesh_data;
    mesh_data.positions.Reserve(120);
    mesh_data.normals.Reserve(120);
    mesh_data.uvs.Reserve(120);
    mesh_data.indices.Reserve(204);

    auto const half_dimensions{parameters.dimensions * 0.5f};
    auto const x{half_dimensions.X};
    auto const y{half_dimensions.Y};
    auto const z{half_dimensions.Z};
    auto const bevel{parameters.bevel_width};

    add_main_face(mesh_data,
                  FVector3f{x, 0.0f, 0.0f},
                  FVector3f::XAxisVector,
                  FVector3f::YAxisVector,
                  FVector3f::ZAxisVector,
                  y,
                  z,
                  bevel);
    add_main_face(mesh_data,
                  FVector3f{-x, 0.0f, 0.0f},
                  -FVector3f::XAxisVector,
                  -FVector3f::YAxisVector,
                  FVector3f::ZAxisVector,
                  y,
                  z,
                  bevel);
    add_main_face(mesh_data,
                  FVector3f{0.0f, y, 0.0f},
                  FVector3f::YAxisVector,
                  -FVector3f::XAxisVector,
                  FVector3f::ZAxisVector,
                  x,
                  z,
                  bevel);
    add_main_face(mesh_data,
                  FVector3f{0.0f, -y, 0.0f},
                  -FVector3f::YAxisVector,
                  FVector3f::XAxisVector,
                  FVector3f::ZAxisVector,
                  x,
                  z,
                  bevel);
    add_main_face(mesh_data,
                  FVector3f{0.0f, 0.0f, z},
                  FVector3f::ZAxisVector,
                  FVector3f::XAxisVector,
                  FVector3f::YAxisVector,
                  x,
                  y,
                  bevel);
    add_main_face(mesh_data,
                  FVector3f{0.0f, 0.0f, -z},
                  -FVector3f::ZAxisVector,
                  FVector3f::XAxisVector,
                  -FVector3f::YAxisVector,
                  x,
                  y,
                  bevel);

    for (float const x_sign : {-1.0f, 1.0f}) {
        for (float const y_sign : {-1.0f, 1.0f}) {
            auto const normal{FVector3f{x_sign, y_sign, 0.0f}.GetSafeNormal()};
            add_polygon(mesh_data,
                        normal,
                        {{x_sign * x, y_sign * (y - bevel), -z + bevel},
                         {x_sign * (x - bevel), y_sign * y, -z + bevel},
                         {x_sign * (x - bevel), y_sign * y, z - bevel},
                         {x_sign * x, y_sign * (y - bevel), z - bevel}});
        }
    }
    for (float const x_sign : {-1.0f, 1.0f}) {
        for (float const z_sign : {-1.0f, 1.0f}) {
            auto const normal{FVector3f{x_sign, 0.0f, z_sign}.GetSafeNormal()};
            add_polygon(mesh_data,
                        normal,
                        {{x_sign * x, -y + bevel, z_sign * (z - bevel)},
                         {x_sign * (x - bevel), -y + bevel, z_sign * z},
                         {x_sign * (x - bevel), y - bevel, z_sign * z},
                         {x_sign * x, y - bevel, z_sign * (z - bevel)}});
        }
    }
    for (float const y_sign : {-1.0f, 1.0f}) {
        for (float const z_sign : {-1.0f, 1.0f}) {
            auto const normal{FVector3f{0.0f, y_sign, z_sign}.GetSafeNormal()};
            add_polygon(mesh_data,
                        normal,
                        {{-x + bevel, y_sign * y, z_sign * (z - bevel)},
                         {-x + bevel, y_sign * (y - bevel), z_sign * z},
                         {x - bevel, y_sign * (y - bevel), z_sign * z},
                         {x - bevel, y_sign * y, z_sign * (z - bevel)}});
        }
    }
    for (float const x_sign : {-1.0f, 1.0f}) {
        for (float const y_sign : {-1.0f, 1.0f}) {
            for (float const z_sign : {-1.0f, 1.0f}) {
                auto const normal{FVector3f{x_sign, y_sign, z_sign}.GetSafeNormal()};
                add_polygon(mesh_data,
                            normal,
                            {{x_sign * x, y_sign * (y - bevel), z_sign * (z - bevel)},
                             {x_sign * (x - bevel), y_sign * y, z_sign * (z - bevel)},
                             {x_sign * (x - bevel), y_sign * (y - bevel), z_sign * z}});
            }
        }
    }

    return mesh_data;
}

}
