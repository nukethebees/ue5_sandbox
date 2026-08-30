#include "SbxMeshGenLab/SphereGenerator.h"

#include <CQTest.h>

namespace SandboxMesh {
namespace {
auto is_finite_sphere_vector(FVector3f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && FMath::IsFinite(value.Z);
}

auto is_valid_sphere_uv(FVector2f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && value.X >= 0.0f &&
           value.X <= 1.0f && value.Y >= 0.0f && value.Y <= 1.0f;
}
}

TEST_CLASS(SphereGenerator, "SandboxMesh.UnitTests")
{
    TEST_METHOD(GeneratesExpectedSegmentDependentBuffers)
    {
        FSbxSphereParameters const parameters{50.0f, 12, 6};
        auto const mesh_data{generate_sphere(parameters)};

        TestRunner->TestEqual(TEXT("Sphere has a seam-duplicated vertex grid"),
                              mesh_data.positions.Num(),
                              (parameters.latitude_segments + 1) *
                                  (parameters.longitude_segments + 1));
        TestRunner->TestEqual(TEXT("Sphere omits degenerate pole triangles"),
                              mesh_data.indices.Num(),
                              parameters.longitude_segments * (parameters.latitude_segments - 1) *
                                  6);
        TestRunner->TestEqual(TEXT("Sphere has a normal per vertex"),
                              mesh_data.normals.Num(),
                              mesh_data.positions.Num());
        TestRunner->TestEqual(
            TEXT("Sphere has a UV per vertex"), mesh_data.uvs.Num(), mesh_data.positions.Num());
    }

    TEST_METHOD(UsesRequestedRadius)
    {
        auto const mesh_data{generate_sphere(FSbxSphereParameters{30.0f, 12, 6})};
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

        TestRunner->TestTrue(TEXT("Minimum bounds use the radius"),
                             minimum.Equals(FVector3f{-30.0f, -30.0f, -30.0f}));
        TestRunner->TestTrue(TEXT("Maximum bounds use the radius"),
                             maximum.Equals(FVector3f{30.0f, 30.0f, 30.0f}));
    }

    TEST_METHOD(GeneratesValidAttributesAndIndices)
    {
        FSbxSphereParameters const parameters{30.0f, 12, 6};
        auto const mesh_data{generate_sphere(parameters)};
        auto const vertex_count{static_cast<uint32>(mesh_data.positions.Num())};

        for (auto const index : mesh_data.indices) {
            TestRunner->TestTrue(TEXT("Triangle index is in range"), index < vertex_count);
        }

        for (int32 vertex_index{0}; vertex_index < mesh_data.positions.Num(); ++vertex_index) {
            auto const normal{mesh_data.normals[vertex_index]};
            TestRunner->TestTrue(TEXT("Normal is finite"), is_finite_sphere_vector(normal));
            TestRunner->TestTrue(TEXT("Normal has unit length"), normal.IsNormalized());
            TestRunner->TestTrue(TEXT("Normal points away from the sphere centre"),
                                 normal.Equals(mesh_data.positions[vertex_index].GetSafeNormal()));
        }

        for (auto const uv : mesh_data.uvs) {
            TestRunner->TestTrue(TEXT("UV is finite and in the unit square"),
                                 is_valid_sphere_uv(uv));
        }
    }

    TEST_METHOD(GeneratesConsistentlyWoundTriangles)
    {
        auto const mesh_data{generate_sphere(FSbxSphereParameters{30.0f, 12, 6})};
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
        }
    }

    TEST_METHOD(GeneratesDeterministicSphereData)
    {
        FSbxSphereParameters const parameters{30.0f, 12, 6};
        auto const first{generate_sphere(parameters)};
        auto const second{generate_sphere(parameters)};

        TestRunner->TestTrue(TEXT("Positions are deterministic"),
                             first.positions == second.positions);
        TestRunner->TestTrue(TEXT("Normals are deterministic"), first.normals == second.normals);
        TestRunner->TestTrue(TEXT("UVs are deterministic"), first.uvs == second.uvs);
        TestRunner->TestTrue(TEXT("Indices are deterministic"), first.indices == second.indices);
    }
};

}
