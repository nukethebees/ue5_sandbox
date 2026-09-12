#include "SbxMeshGenLab/MeshAssembly.h"

#include <span>

namespace SandboxMesh {
namespace {

[[nodiscard]] auto as_span(TArray<FSbxMeshAssemblyPart> const& parts)
    -> std::span<FSbxMeshAssemblyPart const> {
    return {parts.GetData(), static_cast<std::size_t>(parts.Num())};
}

}

auto validate_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FString {
    if (parts.IsEmpty()) {
        return TEXT("Add at least one part to the assembly.");
    }

    auto const part_count{parts.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto const& part{parts[part_index]};
        auto const mesh_error{validate_mesh_request(part.mesh)};
        if (!mesh_error.IsEmpty()) {
            return FString::Printf(TEXT("Part %d: %s"), part_index + 1, *mesh_error);
        }
        if (part.transform.scale.x <= 0.0f || part.transform.scale.y <= 0.0f ||
            part.transform.scale.z <= 0.0f) {
            return FString::Printf(TEXT("Part %d: all scale values must be greater than zero."),
                                   part_index + 1);
        }
    }
    return {};
}

auto generate_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FSbxMeshData {
    check(validate_mesh_assembly(parts).IsEmpty());
    return mesh_gen::generate_assembly(as_span(parts));
}

auto describe_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FString {
    auto const description{mesh_gen::describe_assembly(as_span(parts))};
    return FString{UTF8_TO_TCHAR(description.c_str())};
}

}
