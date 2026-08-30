#include "SbxMeshGenLab/HexTileGenerator.h"

#include <CQTest.h>

namespace SandboxMesh {
namespace {
auto is_finite_hex_tile_vector(FVector3f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && FMath::IsFinite(value.Z);
}

auto is_valid_hex_tile_uv(FVector2f const value) -> bool {
    return FMath::IsFinite(value.X) && FMath::IsFinite(value.Y) && value.X >= 0.0f &&
           value.X <= 1.0f && value.Y >= 0.0f && value.Y <= 1.0f;
}
}

TEST_CLASS(HexTileGenerator, "SandboxMesh.UnitTests")
{
    TEST_METHOD(GeneratesExpectedBuffers)
    {
        auto const mesh_data{generate_hex_tile()};

        TestRunner->TestEqual(TEXT("Hex tile has the expected duplicated vertices"),
                              mesh_data.positions.Num(),
                              6 * (2 * 3 + 3 * 4));
        TestRunner->TestEqual(TEXT("Hex tile has 48 triangles"), mesh_data.indices.Num(), 48 * 3);
        TestRunner->TestEqual(TEXT("Hex tile has a normal per vertex"),
                              mesh_data.normals.Num(),
                              mesh_data.positions.Num());
        TestRunner->TestEqual(
            TEXT("Hex tile has a UV per vertex"), mesh_data.uvs.Num(), mesh_data.positions.Num());
    }

    TEST_METHOD(UsesRequestedDimensionsAndBevel)
    {
        FSbxHexTileParameters const parameters{40.0f, 16.0f, 3.0f, false};
        auto const mesh_data{generate_hex_tile(parameters)};
        auto minimum{mesh_data.positions[0]};
        auto maximum{mesh_data.positions[0]};
        auto found_top_face_ring{false};
        auto found_top_wall_ring{false};

        for (auto const position : mesh_data.positions) {
            minimum.X = FMath::Min(minimum.X, position.X);
            minimum.Y = FMath::Min(minimum.Y, position.Y);
            minimum.Z = FMath::Min(minimum.Z, position.Z);
            maximum.X = FMath::Max(maximum.X, position.X);
            maximum.Y = FMath::Max(maximum.Y, position.Y);
            maximum.Z = FMath::Max(maximum.Z, position.Z);

            auto const radius{FVector2f{position.X, position.Y}.Size()};
            found_top_face_ring |= FMath::IsNearlyEqual(position.Z, 8.0f) &&
                                   FMath::IsNearlyEqual(radius, 37.0f, 0.001f);
            found_top_wall_ring |= FMath::IsNearlyEqual(position.Z, 5.0f) &&
                                   FMath::IsNearlyEqual(radius, 40.0f, 0.001f);
        }

        auto const half_height{parameters.outer_radius * FMath::Sqrt(3.0f) * 0.5f};
        TestRunner->TestTrue(TEXT("Flat-top bounds use the outer radius and depth"),
                             minimum.Equals(FVector3f{-40.0f, -half_height, -8.0f}, 0.001f) &&
                                 maximum.Equals(FVector3f{40.0f, half_height, 8.0f}, 0.001f));
        TestRunner->TestTrue(TEXT("Top face is inset by the bevel width"), found_top_face_ring);
        TestRunner->TestTrue(TEXT("Outer wall begins one bevel width below the face"),
                             found_top_wall_ring);
    }

    TEST_METHOD(PointyTopRotatesTheHexagon)
    {
        auto const mesh_data{generate_hex_tile(FSbxHexTileParameters{40.0f, 16.0f, 3.0f, true})};
        auto maximum_x{0.0f};
        auto maximum_y{0.0f};

        for (auto const position : mesh_data.positions) {
            maximum_x = FMath::Max(maximum_x, FMath::Abs(position.X));
            maximum_y = FMath::Max(maximum_y, FMath::Abs(position.Y));
        }

        constexpr auto bounds_tolerance{0.001f};
        TestRunner->TestTrue(TEXT("Pointy-top orientation has its full radius on Y"),
                             FMath::IsNearlyEqual(maximum_y, 40.0f, bounds_tolerance));
        TestRunner->TestTrue(
            TEXT("Pointy-top orientation has an apothem on X"),
            FMath::IsNearlyEqual(maximum_x, 40.0f * FMath::Sqrt(3.0f) * 0.5f, bounds_tolerance));
    }

    TEST_METHOD(GeneratesValidAttributesIndicesAndWinding)
    {
        auto const mesh_data{generate_hex_tile()};
        auto const vertex_count{static_cast<uint32>(mesh_data.positions.Num())};

        for (auto const index : mesh_data.indices) {
            TestRunner->TestTrue(TEXT("Triangle index is in range"), index < vertex_count);
        }
        for (auto const normal : mesh_data.normals) {
            TestRunner->TestTrue(TEXT("Normal is finite"), is_finite_hex_tile_vector(normal));
            TestRunner->TestTrue(TEXT("Normal has unit length"), normal.IsNormalized());
        }
        for (auto const uv : mesh_data.uvs) {
            TestRunner->TestTrue(TEXT("UV is finite and in the unit square"),
                                 is_valid_hex_tile_uv(uv));
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

    TEST_METHOD(GeneratesDeterministicHexTileData)
    {
        FSbxHexTileParameters const parameters{40.0f, 16.0f, 3.0f, true};
        auto const first{generate_hex_tile(parameters)};
        auto const second{generate_hex_tile(parameters)};

        TestRunner->TestTrue(TEXT("Positions are deterministic"),
                             first.positions == second.positions);
        TestRunner->TestTrue(TEXT("Normals are deterministic"), first.normals == second.normals);
        TestRunner->TestTrue(TEXT("UVs are deterministic"), first.uvs == second.uvs);
        TestRunner->TestTrue(TEXT("Indices are deterministic"), first.indices == second.indices);
    }
};

}
