#include "SbxMeshGenLab/HoneycombPanelGenerator.h"

#include <CQTest.h>

namespace SandboxMesh {
namespace {
auto is_finite_honeycomb_vector(FVector3f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && FMath::IsFinite(value.Z);
}

auto is_valid_honeycomb_uv(FVector2f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && value.X >= 0.0f &&
           value.X <= 1.0f && value.Y >= 0.0f && value.Y <= 1.0f;
}

auto honeycomb_xy_extent(FSbxMeshData const& mesh_data) -> FVector2f {
    FVector2f extent{};
    for (auto const position : mesh_data.positions) {
        extent.X = FMath::Max(extent.X, FMath::Abs(position.X));
        extent.Y = FMath::Max(extent.Y, FMath::Abs(position.Y));
    }
    return extent;
}
}

TEST_CLASS(HoneycombPanelGenerator, "SandboxMesh.UnitTests")
{
    TEST_METHOD(GeneratesExpectedSingleCellBuffers)
    {
        auto const mesh_data{generate_honeycomb_panel(
            FSbxHoneycombPanelParameters{1, 1, 40.0f, 6.0f, 10.0f, false})};

        constexpr int32 edge_count{6};
        TestRunner->TestEqual(
            TEXT("One cell has six wall segments"), mesh_data.positions.Num(), edge_count * 24);
        TestRunner->TestEqual(TEXT("Each wall segment has twelve triangles"),
                              mesh_data.indices.Num(),
                              edge_count * 36);
        TestRunner->TestEqual(TEXT("Honeycomb has a normal per vertex"),
                              mesh_data.normals.Num(),
                              mesh_data.positions.Num());
        TestRunner->TestEqual(
            TEXT("Honeycomb has a UV per vertex"), mesh_data.uvs.Num(), mesh_data.positions.Num());
    }

    TEST_METHOD(SharesInternalCellEdges)
    {
        auto const mesh_data{generate_honeycomb_panel(
            FSbxHoneycombPanelParameters{2, 2, 40.0f, 6.0f, 10.0f, false})};

        constexpr int32 unique_edge_count{19};
        TestRunner->TestEqual(TEXT("A two-by-two panel emits each shared edge once"),
                              mesh_data.positions.Num(),
                              unique_edge_count * 24);
        TestRunner->TestEqual(TEXT("Shared-edge topology has the expected indices"),
                              mesh_data.indices.Num(),
                              unique_edge_count * 36);
    }

    TEST_METHOD(UsesRequestedDepthAndLeavesCellCentresOpen)
    {
        FSbxHoneycombPanelParameters const parameters{1, 1, 40.0f, 6.0f, 12.0f, false};
        auto const mesh_data{generate_honeycomb_panel(parameters)};
        auto minimum_z{TNumericLimits<float>::Max()};
        auto maximum_z{TNumericLimits<float>::Lowest()};
        auto minimum_radius{TNumericLimits<float>::Max()};
        for (auto const position : mesh_data.positions) {
            minimum_z = FMath::Min(minimum_z, position.Z);
            maximum_z = FMath::Max(maximum_z, position.Z);
            minimum_radius = FMath::Min(minimum_radius, FVector2f{position.X, position.Y}.Size());
        }

        TestRunner->TestTrue(TEXT("Panel uses the requested depth"),
                             FMath::IsNearlyEqual(minimum_z, -6.0f) &&
                                 FMath::IsNearlyEqual(maximum_z, 6.0f));
        TestRunner->TestTrue(TEXT("The centre of a cell remains open"),
                             minimum_radius > parameters.cell_radius * 0.5f);
    }

    TEST_METHOD(PointyTopRotatesASingleCell)
    {
        auto const flat{generate_honeycomb_panel(
            FSbxHoneycombPanelParameters{1, 1, 40.0f, 6.0f, 10.0f, false})};
        auto const pointy{
            generate_honeycomb_panel(FSbxHoneycombPanelParameters{1, 1, 40.0f, 6.0f, 10.0f, true})};
        auto const flat_extent{honeycomb_xy_extent(flat)};
        auto const pointy_extent{honeycomb_xy_extent(pointy)};

        TestRunner->TestTrue(TEXT("Pointy-top orientation rotates the X extent into Y"),
                             FMath::IsNearlyEqual(flat_extent.X, pointy_extent.Y, 0.001f));
        TestRunner->TestTrue(TEXT("Pointy-top orientation rotates the Y extent into X"),
                             FMath::IsNearlyEqual(flat_extent.Y, pointy_extent.X, 0.001f));
    }

    TEST_METHOD(GeneratesValidAttributesIndicesAndWinding)
    {
        auto const mesh_data{generate_honeycomb_panel()};
        auto const vertex_count{static_cast<uint32>(mesh_data.positions.Num())};

        for (auto const index : mesh_data.indices) {
            TestRunner->TestTrue(TEXT("Triangle index is in range"), index < vertex_count);
        }
        for (auto const normal : mesh_data.normals) {
            TestRunner->TestTrue(TEXT("Normal is finite"), is_finite_honeycomb_vector(normal));
            TestRunner->TestTrue(TEXT("Normal has unit length"), normal.IsNormalized());
        }
        for (auto const uv : mesh_data.uvs) {
            TestRunner->TestTrue(TEXT("UV is finite and in the unit square"),
                                 is_valid_honeycomb_uv(uv));
        }

        auto const triangle_count{mesh_data.indices.Num() / 3};
        for (int32 triangle_index{0}; triangle_index < triangle_count; ++triangle_index) {
            auto const first_index{mesh_data.indices[triangle_index * 3]};
            auto const second_index{mesh_data.indices[triangle_index * 3 + 1]};
            auto const third_index{mesh_data.indices[triangle_index * 3 + 2]};
            auto const& first_position{mesh_data.positions[first_index]};
            auto const& second_position{mesh_data.positions[second_index]};
            auto const& third_position{mesh_data.positions[third_index]};
            auto const front_face_normal{FVector3f::CrossProduct(third_position - first_position,
                                                                 second_position - first_position)};

            TestRunner->TestTrue(TEXT("Triangle is not degenerate"),
                                 front_face_normal.SizeSquared() > 0.0f);
            TestRunner->TestTrue(
                TEXT("Vertex normal agrees with Unreal front-face winding"),
                FVector3f::DotProduct(front_face_normal, mesh_data.normals[first_index]) > 0.0f);
        }
    }

    TEST_METHOD(GeneratesDeterministicHoneycombData)
    {
        FSbxHoneycombPanelParameters const parameters{3, 4, 35.0f, 5.0f, 14.0f, true};
        auto const first{generate_honeycomb_panel(parameters)};
        auto const second{generate_honeycomb_panel(parameters)};

        TestRunner->TestTrue(TEXT("Positions are deterministic"),
                             first.positions == second.positions);
        TestRunner->TestTrue(TEXT("Normals are deterministic"), first.normals == second.normals);
        TestRunner->TestTrue(TEXT("UVs are deterministic"), first.uvs == second.uvs);
        TestRunner->TestTrue(TEXT("Indices are deterministic"), first.indices == second.indices);
    }
};

}
