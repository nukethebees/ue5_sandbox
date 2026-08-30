#include "SbxMeshGenLab/HoneycombPanelGenerator.h"

#include "Containers/Set.h"
#include "Containers/StaticArray.h"

namespace SandboxMesh {
namespace {
constexpr int32 honeycomb_side_count{6};

struct FEdgeKey {
    int64 midpoint_x{};
    int64 midpoint_y{};
    int32 orientation{};

    auto operator==(FEdgeKey const&) const -> bool = default;
};

auto GetTypeHash(FEdgeKey const& key) -> uint32 {
    return HashCombineFast(
        HashCombineFast(::GetTypeHash(key.midpoint_x), ::GetTypeHash(key.midpoint_y)),
        ::GetTypeHash(key.orientation));
}

void add_honeycomb_quad(FSbxMeshData& mesh_data,
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

void add_wall_segment(FSbxMeshData& mesh_data,
                      FVector2f const start,
                      FVector2f const end,
                      float const wall_thickness,
                      float const depth) {
    auto const direction{(end - start).GetSafeNormal()};
    auto const perpendicular{FVector2f{-direction.Y, direction.X}};
    auto const half_width{wall_thickness * 0.5f};
    auto const half_depth{depth * 0.5f};
    auto const extended_start{start - direction * half_width};
    auto const extended_end{end + direction * half_width};
    auto const start_left{extended_start + perpendicular * half_width};
    auto const start_right{extended_start - perpendicular * half_width};
    auto const end_left{extended_end + perpendicular * half_width};
    auto const end_right{extended_end - perpendicular * half_width};

    auto const make_position = [](FVector2f const position, float const z) {
        return FVector3f{position.X, position.Y, z};
    };
    auto const top_start_left{make_position(start_left, half_depth)};
    auto const top_start_right{make_position(start_right, half_depth)};
    auto const top_end_left{make_position(end_left, half_depth)};
    auto const top_end_right{make_position(end_right, half_depth)};
    auto const bottom_start_left{make_position(start_left, -half_depth)};
    auto const bottom_start_right{make_position(start_right, -half_depth)};
    auto const bottom_end_left{make_position(end_left, -half_depth)};
    auto const bottom_end_right{make_position(end_right, -half_depth)};
    auto const direction_3d{FVector3f{direction.X, direction.Y, 0.0f}};
    auto const perpendicular_3d{FVector3f{perpendicular.X, perpendicular.Y, 0.0f}};

    add_honeycomb_quad(mesh_data,
                       top_start_right,
                       top_end_right,
                       top_end_left,
                       top_start_left,
                       FVector3f::ZAxisVector);
    add_honeycomb_quad(mesh_data,
                       bottom_start_left,
                       bottom_end_left,
                       bottom_end_right,
                       bottom_start_right,
                       -FVector3f::ZAxisVector);
    add_honeycomb_quad(mesh_data,
                       bottom_start_left,
                       top_start_left,
                       top_end_left,
                       bottom_end_left,
                       perpendicular_3d);
    add_honeycomb_quad(mesh_data,
                       bottom_start_right,
                       bottom_end_right,
                       top_end_right,
                       top_start_right,
                       -perpendicular_3d);
    add_honeycomb_quad(mesh_data,
                       bottom_start_left,
                       bottom_start_right,
                       top_start_right,
                       top_start_left,
                       -direction_3d);
    add_honeycomb_quad(
        mesh_data, bottom_end_right, bottom_end_left, top_end_left, top_end_right, direction_3d);
}

auto make_cell_center(int32 const row,
                      int32 const column,
                      float const cell_radius,
                      bool const pointy_top) -> FVector2f {
    auto const root_three{FMath::Sqrt(3.0f)};
    if (pointy_top) {
        return FVector2f{root_three * cell_radius *
                             (static_cast<float>(column) + 0.5f * static_cast<float>(row & 1)),
                         1.5f * cell_radius * static_cast<float>(row)};
    }

    return FVector2f{1.5f * cell_radius * static_cast<float>(column),
                     root_three * cell_radius *
                         (static_cast<float>(row) + 0.5f * static_cast<float>(column & 1))};
}

auto make_edge_key(FVector2f const start, FVector2f const end, int32 const side_index) -> FEdgeKey {
    constexpr double quantization_scale{1000.0};
    auto const midpoint{(start + end) * 0.5f};
    return FEdgeKey{FMath::RoundToInt64(static_cast<double>(midpoint.X) * quantization_scale),
                    FMath::RoundToInt64(static_cast<double>(midpoint.Y) * quantization_scale),
                    side_index % 3};
}
}

auto generate_honeycomb_panel(FSbxHoneycombPanelParameters const& parameters) -> FSbxMeshData {
    check(parameters.rows > 0);
    check(parameters.columns > 0);
    check(parameters.cell_radius > 0.0f);
    check(parameters.wall_thickness > 0.0f);
    check(parameters.wall_thickness < parameters.cell_radius);
    check(parameters.depth > 0.0f);

    TArray<FVector2f> centers;
    centers.Reserve(parameters.rows * parameters.columns);
    auto minimum{FVector2f{TNumericLimits<float>::Max(), TNumericLimits<float>::Max()}};
    auto maximum{FVector2f{TNumericLimits<float>::Lowest(), TNumericLimits<float>::Lowest()}};
    for (int32 row{0}; row < parameters.rows; ++row) {
        for (int32 column{0}; column < parameters.columns; ++column) {
            auto const center{
                make_cell_center(row, column, parameters.cell_radius, parameters.pointy_top)};
            centers.Add(center);
            minimum.X = FMath::Min(minimum.X, center.X);
            minimum.Y = FMath::Min(minimum.Y, center.Y);
            maximum.X = FMath::Max(maximum.X, center.X);
            maximum.Y = FMath::Max(maximum.Y, center.Y);
        }
    }
    auto const panel_center{(minimum + maximum) * 0.5f};

    auto const cell_count{parameters.rows * parameters.columns};
    auto const maximum_edge_count{cell_count * honeycomb_side_count};
    constexpr int32 faces_per_segment{6};
    constexpr int32 vertices_per_face{4};
    constexpr int32 indices_per_face{6};
    FSbxMeshData mesh_data{};
    mesh_data.positions.Reserve(maximum_edge_count * faces_per_segment * vertices_per_face);
    mesh_data.normals.Reserve(maximum_edge_count * faces_per_segment * vertices_per_face);
    mesh_data.uvs.Reserve(maximum_edge_count * faces_per_segment * vertices_per_face);
    mesh_data.indices.Reserve(maximum_edge_count * faces_per_segment * indices_per_face);

    TSet<FEdgeKey> generated_edges;
    auto const angle_offset{parameters.pointy_top ? UE_PI * 0.5f : 0.0f};
    for (auto const uncentered_cell_center : centers) {
        auto const cell_center{uncentered_cell_center - panel_center};
        TStaticArray<FVector2f, honeycomb_side_count> corners{};
        for (int32 side_index{0}; side_index < honeycomb_side_count; ++side_index) {
            auto const angle{angle_offset + UE_TWO_PI * static_cast<float>(side_index) /
                                                static_cast<float>(honeycomb_side_count)};
            corners[side_index] =
                cell_center + FVector2f{parameters.cell_radius * FMath::Cos(angle),
                                        parameters.cell_radius * FMath::Sin(angle)};
        }

        for (int32 side_index{0}; side_index < honeycomb_side_count; ++side_index) {
            auto const next_index{(side_index + 1) % honeycomb_side_count};
            auto const edge_key{
                make_edge_key(corners[side_index], corners[next_index], side_index)};
            if (generated_edges.Contains(edge_key)) {
                continue;
            }

            generated_edges.Add(edge_key);
            add_wall_segment(mesh_data,
                             corners[side_index],
                             corners[next_index],
                             parameters.wall_thickness,
                             parameters.depth);
        }
    }

    return mesh_data;
}

}
