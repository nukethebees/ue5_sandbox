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
    if (auto const error{mesh_gen::validate_assembly(as_span(parts))}; !error.empty()) {
        return FString{UTF8_TO_TCHAR(error.c_str())};
    }

    auto const part_count{parts.Num()};
    for (int32 part_index{}; part_index < part_count; ++part_index) {
        auto const& part{parts[part_index]};
        auto const mesh_error{validate_mesh_asset_name(part.mesh.asset_name)};
        if (!mesh_error.IsEmpty()) {
            return FString::Printf(TEXT("Part %d: %s"), part_index + 1, *mesh_error);
        }
    }
    return {};
}

auto generate_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FSbxMeshData {
    return mesh_gen::generate_assembly(as_span(parts));
}

auto describe_mesh_assembly(TArray<FSbxMeshAssemblyPart> const& parts) -> FString {
    auto const description{mesh_gen::describe_assembly(as_span(parts))};
    return FString{UTF8_TO_TCHAR(description.c_str())};
}

}
