#include "SbxMeshGenLab/WedgeGenerator.h"

#include <CQTest.h>

namespace SandboxMesh {
namespace {
auto has_valid_wedge_geometry(FSbxMeshData const& mesh_data) -> bool {
    if (mesh_data.positions.IsEmpty() || mesh_data.positions.Num() != mesh_data.normals.Num() ||
        mesh_data.positions.Num() != mesh_data.uvs.Num() || mesh_data.indices.Num() % 3 != 0) {
        return false;
    }

    auto const vertex_count{static_cast<uint32>(mesh_data.positions.Num())};
    auto const triangle_count{mesh_data.indices.Num() / 3};
    for (int32 triangle_index{}; triangle_index < triangle_count; ++triangle_index) {
        auto const first_index{mesh_data.indices[triangle_index * 3]};
        auto const second_index{mesh_data.indices[triangle_index * 3 + 1]};
        auto const third_index{mesh_data.indices[triangle_index * 3 + 2]};
        if (first_index >= vertex_count || second_index >= vertex_count ||
            third_index >= vertex_count) {
            return false;
        }

        auto const normal{mesh_data.normals[first_index]};
        auto const uv{mesh_data.uvs[first_index]};
        auto const winding_normal{FVector3f::CrossProduct(
            mesh_data.positions[third_index] - mesh_data.positions[first_index],
            mesh_data.positions[second_index] - mesh_data.positions[first_index])};
        if (!normal.IsNormalized() || !FMath::IsFinite(uv.X) || !FMath::IsFinite(uv.Y) ||
            uv.X < 0.0f || uv.X > 1.0f || uv.Y < 0.0f || uv.Y > 1.0f ||
            winding_normal.SizeSquared() <= 0.0f ||
            FVector3f::DotProduct(winding_normal, normal) <= 0.0f) {
            return false;
        }
    }
    return true;
}
}

TEST_CLASS(WedgeGenerator, "SandboxMesh.UnitTests")
{
    TEST_METHOD(GeneratesExpectedValidGeometry)
    {
        auto const mesh_data{generate_wedge()};

        TestRunner->TestEqual(
            TEXT("Wedge has the expected vertices"), mesh_data.positions.Num(), 24);
        TestRunner->TestEqual(
            TEXT("Wedge has the expected triangles"), mesh_data.indices.Num(), 36);
        TestRunner->TestTrue(TEXT("Wedge attributes and winding are valid"),
                             has_valid_wedge_geometry(mesh_data));
    }

    TEST_METHOD(UsesRequestedDimensionsAndOffsetAndIsDeterministic)
    {
        FSbxWedgeParameters const parameters{FVector3f{120.0f, 80.0f, 40.0f}, 30.0f, 15.0f};
        auto const first{generate_wedge(parameters)};
        auto const second{generate_wedge(parameters)};

        FBox3f bounds{ForceInit};
        for (auto const position : first.positions) {
            bounds += position;
        }
        TestRunner->TestTrue(TEXT("Wedge uses requested dimensions"),
                             bounds.Min.Equals(FVector3f{-60.0f, -40.0f, -20.0f}) &&
                                 bounds.Max.Equals(FVector3f{60.0f, 40.0f, 20.0f}));
        TestRunner->TestTrue(TEXT("Wedge top uses requested length and offset"),
                             first.positions.Contains(FVector3f{0.0f, -40.0f, 20.0f}) &&
                                 first.positions.Contains(FVector3f{30.0f, -40.0f, 20.0f}));
        TestRunner->TestTrue(TEXT("Wedge generation is deterministic"),
                             first.positions == second.positions &&
                                 first.normals == second.normals && first.uvs == second.uvs &&
                                 first.indices == second.indices);
    }
};

}
