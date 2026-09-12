#include "SbxMeshGenLab/MeshGenerationRequest.h"

#include "Misc/PackageName.h"

namespace SandboxMesh {

auto to_native(ESbxMeshShape const shape) -> mesh_gen::Shape {
    return static_cast<mesh_gen::Shape>(shape);
}

auto to_unreal(mesh_gen::Shape const shape) -> ESbxMeshShape {
    return static_cast<ESbxMeshShape>(shape);
}

auto make_default_mesh_request(ESbxMeshShape const shape) -> FSbxMeshGenerationRequest {
    return mesh_gen::make_default_request(to_native(shape));
}

auto validate_mesh_request(FSbxMeshGenerationRequest const& request) -> FString {
    auto const asset_name{FString{UTF8_TO_TCHAR(request.asset_name.c_str())}};
    auto const object_path{
        FString::Printf(TEXT("/SandboxMesh/MeshGenLab/Generated/%s.%s"), *asset_name, *asset_name)};
    FText invalid_name_reason;
    if (asset_name.IsEmpty() ||
        !FPackageName::IsValidObjectPath(object_path, &invalid_name_reason)) {
        return FString::Printf(
            TEXT("Invalid asset name '%s': %s"), *asset_name, *invalid_name_reason.ToString());
    }

    auto const error{mesh_gen::validate_request(request)};
    return FString{UTF8_TO_TCHAR(error.c_str())};
}

auto describe_mesh_request(FSbxMeshGenerationRequest const& request) -> FString {
    auto const description{mesh_gen::describe_request(request)};
    return FString{UTF8_TO_TCHAR(description.c_str())};
}

}
