#include "SbxMeshGenLab/ConeGenerator.h"

namespace SandboxMesh {
namespace {
auto make_side_normal(FSbxConeParameters const& parameters, float const angle) -> FVector3f {
    return FVector3f{parameters.height * FMath::Cos(angle),
                     parameters.height * FMath::Sin(angle),
                     parameters.radius}
        .GetSafeNormal();
}

void add_side(FSbxMeshData& mesh_data, FSbxConeParameters const& parameters) {
    auto const half_height{parameters.height * 0.5f};
    for (int32 segment_index{0}; segment_index <= parameters.radial_segments; ++segment_index) {
        auto const fraction{static_cast<float>(segment_index) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{UE_TWO_PI * fraction};

        mesh_data.positions.Add(FVector3f{parameters.radius * FMath::Cos(angle),
                                          parameters.radius * FMath::Sin(angle),
                                          -half_height});
        mesh_data.normals.Add(make_side_normal(parameters, angle));
        mesh_data.uvs.Add(FVector2f{fraction, 1.0f});
    }

    auto const apex_start{static_cast<uint32>(mesh_data.positions.Num())};
    for (int32 segment_index{0}; segment_index < parameters.radial_segments; ++segment_index) {
        auto const fraction{(static_cast<float>(segment_index) + 0.5f) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{UE_TWO_PI * fraction};

        mesh_data.positions.Add(FVector3f{0.0f, 0.0f, half_height});
        mesh_data.normals.Add(make_side_normal(parameters, angle));
        mesh_data.uvs.Add(FVector2f{fraction, 0.0f});
    }

    for (int32 segment_index{0}; segment_index < parameters.radial_segments; ++segment_index) {
        auto const current{static_cast<uint32>(segment_index)};
        auto const next{current + 1};
        auto const apex{apex_start + current};
        mesh_data.indices.Append({current, apex, next});
    }
}

void add_base(FSbxMeshData& mesh_data, FSbxConeParameters const& parameters) {
    auto const half_height{parameters.height * 0.5f};
    auto const centre_index{static_cast<uint32>(mesh_data.positions.Num())};
    mesh_data.positions.Add(FVector3f{0.0f, 0.0f, -half_height});
    mesh_data.normals.Add(-FVector3f::ZAxisVector);
    mesh_data.uvs.Add(FVector2f{0.5f, 0.5f});

    for (int32 segment_index{0}; segment_index < parameters.radial_segments; ++segment_index) {
        auto const fraction{static_cast<float>(segment_index) /
                            static_cast<float>(parameters.radial_segments)};
        auto const angle{UE_TWO_PI * fraction};
        auto const cosine{FMath::Cos(angle)};
        auto const sine{FMath::Sin(angle)};
        mesh_data.positions.Add(
            FVector3f{parameters.radius * cosine, parameters.radius * sine, -half_height});
        mesh_data.normals.Add(-FVector3f::ZAxisVector);
        mesh_data.uvs.Add(FVector2f{0.5f + cosine * 0.5f, 0.5f + sine * 0.5f});
    }

    for (int32 segment_index{0}; segment_index < parameters.radial_segments; ++segment_index) {
        auto const current{centre_index + 1 + static_cast<uint32>(segment_index)};
        auto const next{centre_index + 1 +
                        static_cast<uint32>((segment_index + 1) % parameters.radial_segments)};
        mesh_data.indices.Append({centre_index, current, next});
    }
}
}

auto generate_cone(FSbxConeParameters const& parameters) -> FSbxMeshData {
    check(parameters.radius > 0.0f);
    check(parameters.height > 0.0f);
    check(parameters.radial_segments >= 3);

    FSbxMeshData mesh_data{};
    auto const vertex_count{parameters.radial_segments * 3 + 2};
    auto const index_count{parameters.radial_segments * 6};
    mesh_data.positions.Reserve(vertex_count);
    mesh_data.normals.Reserve(vertex_count);
    mesh_data.uvs.Reserve(vertex_count);
    mesh_data.indices.Reserve(index_count);

    add_side(mesh_data, parameters);
    add_base(mesh_data, parameters);

    return mesh_data;
}

}
