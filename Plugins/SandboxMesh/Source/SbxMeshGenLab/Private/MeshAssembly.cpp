#include "SbxMeshGenLab/MeshAssembly.h"

namespace SandboxMesh {

void append_transformed_mesh(FSbxMeshData& destination,
                             FSbxMeshData const& source,
                             FSbxMeshTransform const& transform) {
    auto const vertex_offset{static_cast<uint32>(destination.positions.Num())};
    auto const rotation{transform.rotation.Quaternion()};

    destination.positions.Reserve(destination.positions.Num() + source.positions.Num());
    destination.normals.Reserve(destination.normals.Num() + source.normals.Num());
    destination.uvs.Reserve(destination.uvs.Num() + source.uvs.Num());
    destination.indices.Reserve(destination.indices.Num() + source.indices.Num());
    auto const source_triangle_count{source.indices.Num() / 3};
    destination.triangle_material_roles.Reserve(destination.triangle_material_roles.Num() +
                                                source_triangle_count);

    for (auto const position : source.positions) {
        destination.positions.Add(rotation.RotateVector(position * transform.scale) +
                                  transform.translation);
    }

    for (auto const normal : source.normals) {
        auto const inverse_scaled_normal{FVector3f{normal.X / transform.scale.X,
                                                   normal.Y / transform.scale.Y,
                                                   normal.Z / transform.scale.Z}};
        destination.normals.Add(rotation.RotateVector(inverse_scaled_normal).GetSafeNormal());
    }

    destination.uvs.Append(source.uvs);
    for (auto const index : source.indices) {
        destination.indices.Add(vertex_offset + index);
    }

    if (source.triangle_material_roles.IsEmpty()) {
        destination.triangle_material_roles.AddDefaulted(source_triangle_count);
    } else {
        check(source.triangle_material_roles.Num() == source_triangle_count);
        destination.triangle_material_roles.Append(source.triangle_material_roles);
    }
}

auto validate_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FString {
    if (parts.IsEmpty()) {
        return TEXT("Add at least one part to the assembly.");
    }

    auto const part_count{parts.Num()};
    for (int32 part_index{0}; part_index < part_count; ++part_index) {
        auto const& part{parts[part_index]};
        auto const mesh_error{validate_mesh_request(part.mesh)};
        if (!mesh_error.IsEmpty()) {
            return FString::Printf(TEXT("Part %d: %s"), part_index + 1, *mesh_error);
        }

        if (part.transform.scale.GetMin() <= 0.0f) {
            return FString::Printf(TEXT("Part %d: all scale values must be greater than zero."),
                                   part_index + 1);
        }
    }

    return {};
}

auto generate_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FSbxMeshData {
    check(validate_mesh_assembly(parts).IsEmpty());

    FSbxMeshData assembly;
    for (auto const& part : parts) {
        append_transformed_mesh(assembly, generate_mesh(part.mesh), part.transform);
    }
    return assembly;
}

auto describe_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FString {
    FString description{TEXT("version=1;assembly")};
    auto const part_count{parts.Num()};
    for (int32 part_index{0}; part_index < part_count; ++part_index) {
        auto const& part{parts[part_index]};
        description += FString::Printf(TEXT(";part%d={%s;translation=%s;rotation=%s;scale=%s}"),
                                       part_index,
                                       *describe_mesh_request(part.mesh),
                                       *part.transform.translation.ToString(),
                                       *part.transform.rotation.ToString(),
                                       *part.transform.scale.ToString());
    }
    return description;
}

}
