#include "SbxMeshGenLab/ConeGenerator.h"

#include <CQTest.h>

namespace SandboxMesh {
namespace {
auto is_finite_cone_vector(FVector3f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && FMath::IsFinite(value.Z);
}

auto is_valid_cone_uv(FVector2f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && value.X >= 0.0f &&
           value.X <= 1.0f && value.Y >= 0.0f && value.Y <= 1.0f;
}
}

TEST_CLASS(ConeGenerator, "SandboxMesh.UnitTests")
{
    TEST_METHOD(GeneratesExpectedSegmentDependentBuffers)
    {
        FSbxConeParameters const parameters{50.0f, 100.0f, 12};
        auto const mesh_data{generate_cone(parameters)};

        TestRunner->TestEqual(TEXT("Cone has side and base vertices"),
                              mesh_data.positions.Num(),
                              parameters.radial_segments * 3 + 2);
        TestRunner->TestEqual(TEXT("Cone has two triangles per segment"),
                              mesh_data.indices.Num(),
                              parameters.radial_segments * 6);
        TestRunner->TestEqual(TEXT("Cone has a normal per vertex"),
                              mesh_data.normals.Num(),
                              mesh_data.positions.Num());
        TestRunner->TestEqual(
            TEXT("Cone has a UV per vertex"), mesh_data.uvs.Num(), mesh_data.positions.Num());
    }

    TEST_METHOD(UsesRequestedRadiusAndHeight)
    {
        auto const mesh_data{generate_cone(FSbxConeParameters{30.0f, 80.0f, 12})};
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

        TestRunner->TestTrue(TEXT("Minimum bounds use radius and negative half-height"),
                             minimum.Equals(FVector3f{-30.0f, -30.0f, -40.0f}));
        TestRunner->TestTrue(TEXT("Maximum bounds use radius and positive half-height"),
                             maximum.Equals(FVector3f{30.0f, 30.0f, 40.0f}));
    }

    TEST_METHOD(GeneratesValidAttributesAndIndices)
    {
        FSbxConeParameters const parameters{30.0f, 80.0f, 12};
        auto const mesh_data{generate_cone(parameters)};
        auto const vertex_count{static_cast<uint32>(mesh_data.positions.Num())};

        for (auto const index : mesh_data.indices) {
            TestRunner->TestTrue(TEXT("Triangle index is in range"), index < vertex_count);
        }

        for (auto const normal : mesh_data.normals) {
            TestRunner->TestTrue(TEXT("Normal is finite"), is_finite_cone_vector(normal));
            TestRunner->TestTrue(TEXT("Normal has unit length"), normal.IsNormalized());
        }

        for (auto const uv : mesh_data.uvs) {
            TestRunner->TestTrue(TEXT("UV is finite and in the unit square"), is_valid_cone_uv(uv));
        }

        auto const side_vertex_count{parameters.radial_segments * 2 + 1};
        for (int32 vertex_index{0}; vertex_index < side_vertex_count; ++vertex_index) {
            TestRunner->TestTrue(TEXT("Side normal points upward"),
                                 mesh_data.normals[vertex_index].Z > 0.0f);
        }
        for (int32 vertex_index{side_vertex_count}; vertex_index < mesh_data.normals.Num();
             ++vertex_index) {
            TestRunner->TestTrue(TEXT("Base normal points downward"),
                                 mesh_data.normals[vertex_index].Equals(-FVector3f::ZAxisVector));
        }
    }

    TEST_METHOD(GeneratesConsistentlyWoundTriangles)
    {
        auto const mesh_data{generate_cone(FSbxConeParameters{30.0f, 80.0f, 12})};
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
            TestRunner->TestTrue(TEXT("Unreal front-face winding points away from the centre"),
                                 FVector3f::DotProduct(front_face_normal, triangle_centre) > 0.0f);
            TestRunner->TestTrue(
                TEXT("Vertex normal agrees with Unreal front-face winding"),
                FVector3f::DotProduct(front_face_normal, mesh_data.normals[first_index]) > 0.0f);
        }
    }

    TEST_METHOD(GeneratesDeterministicConeData)
    {
        FSbxConeParameters const parameters{30.0f, 80.0f, 12};
        auto const first{generate_cone(parameters)};
        auto const second{generate_cone(parameters)};

        TestRunner->TestTrue(TEXT("Positions are deterministic"),
                             first.positions == second.positions);
        TestRunner->TestTrue(TEXT("Normals are deterministic"), first.normals == second.normals);
        TestRunner->TestTrue(TEXT("UVs are deterministic"), first.uvs == second.uvs);
        TestRunner->TestTrue(TEXT("Indices are deterministic"), first.indices == second.indices);
    }
};

}
