#include "SbxMeshGenLab/BoxGenerator.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"

#include <CQTest.h>

namespace SandboxMesh {
namespace {
auto is_finite(FVector3f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && FMath::IsFinite(value.Z);
}

auto is_valid_uv(FVector2f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && value.X >= 0.0f &&
           value.X <= 1.0f && value.Y >= 0.0f && value.Y <= 1.0f;
}
}

TEST_CLASS(BoxGenerator, "SandboxMesh.UnitTests")
{
    TEST_METHOD(GeneratesExpectedBoxBuffers)
    {
        auto const mesh_data{generate_box()};

        TestRunner->TestEqual(TEXT("Box has 24 face vertices"), mesh_data.positions.Num(), 24);
        TestRunner->TestEqual(TEXT("Box has 36 triangle indices"), mesh_data.indices.Num(), 36);
        TestRunner->TestEqual(TEXT("Box has a normal per vertex"),
                              mesh_data.normals.Num(),
                              mesh_data.positions.Num());
        TestRunner->TestEqual(
            TEXT("Box has a UV per vertex"), mesh_data.uvs.Num(), mesh_data.positions.Num());
    }

    TEST_METHOD(UsesRequestedDimensions)
    {
        FSbxBoxParameters const parameters{FVector3f{120.0f, 80.0f, 40.0f}};
        auto const mesh_data{generate_box(parameters)};
        auto minimum{mesh_data.positions[0]};
        auto maximum{mesh_data.positions[0]};

        for (auto const position : mesh_data.positions) {
            minimum.X = FMath::Min(minimum.X, position.X);
            minimum.Y = FMath::Min(minimum.Y, position.Y);
            minimum.Z = FMath::Min(minimum.Z, position.Z);
            maximum.X = FMath::Max(maximum.X, position.X);
            maximum.Y = FMath::Max(maximum.Y, position.Y);
            maximum.Z = FMath::Max(maximum.Z, position.Z);
        }

        TestRunner->TestTrue(TEXT("Minimum bounds are negative half-dimensions"),
                             minimum.Equals(FVector3f{-60.0f, -40.0f, -20.0f}));
        TestRunner->TestTrue(TEXT("Maximum bounds are positive half-dimensions"),
                             maximum.Equals(FVector3f{60.0f, 40.0f, 20.0f}));
    }

    TEST_METHOD(GeneratesValidIndexedGeometry)
    {
        auto const mesh_data{generate_box(FSbxBoxParameters{FVector3f{120.0f, 80.0f, 40.0f}})};
        auto const vertex_count{static_cast<uint32>(mesh_data.positions.Num())};

        for (auto const index : mesh_data.indices) {
            TestRunner->TestTrue(TEXT("Triangle index is in range"), index < vertex_count);
        }

        for (auto const normal : mesh_data.normals) {
            TestRunner->TestTrue(TEXT("Normal is finite"), is_finite(normal));
            TestRunner->TestTrue(TEXT("Normal has unit length"),
                                 FMath::IsNearlyEqual(normal.SizeSquared(), 1.0f));
        }

        for (auto const uv : mesh_data.uvs) {
            TestRunner->TestTrue(TEXT("UV is finite and in the unit square"), is_valid_uv(uv));
        }
    }

    TEST_METHOD(GeneratesConsistentlyWoundTriangles)
    {
        auto const mesh_data{generate_box(FSbxBoxParameters{FVector3f{120.0f, 80.0f, 40.0f}})};
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
            auto const triangle_centre{(first_position + second_position + third_position) / 3.0f};

            TestRunner->TestTrue(TEXT("Triangle is not degenerate"),
                                 front_face_normal.SizeSquared() > 0.0f);
            TestRunner->TestTrue(TEXT("Unreal front-face winding points away from the box centre"),
                                 FVector3f::DotProduct(front_face_normal, triangle_centre) > 0.0f);
            TestRunner->TestTrue(
                TEXT("Vertex normal agrees with Unreal front-face winding"),
                FVector3f::DotProduct(front_face_normal, mesh_data.normals[first_index]) > 0.0f);
        }
    }

    TEST_METHOD(GeneratesDeterministicBoxData)
    {
        FSbxBoxParameters const parameters{FVector3f{120.0f, 80.0f, 40.0f}};
        auto const first{generate_box(parameters)};
        auto const second{generate_box(parameters)};

        TestRunner->TestTrue(TEXT("Positions are deterministic"),
                             first.positions == second.positions);
        TestRunner->TestTrue(TEXT("Normals are deterministic"), first.normals == second.normals);
        TestRunner->TestTrue(TEXT("UVs are deterministic"), first.uvs == second.uvs);
        TestRunner->TestTrue(TEXT("Indices are deterministic"), first.indices == second.indices);
    }
};

TEST_CLASS(MeshGenLabUi, "SandboxMesh.UnitTests")
{
    TEST_METHOD(OpensAndClosesTheLabTab)
    {
        auto const tab{FGlobalTabmanager::Get()->TryInvokeTab(FName{TEXT("SandboxMesh.GenLab")})};

        TestRunner->TestTrue(TEXT("Mesh Gen Lab tab opens"), tab.IsValid());
        if (tab.IsValid()) {
            tab->RequestCloseTab();
        }
    }
};

}
