#include "Commandlets/GenerateSandboxMeshHexTileCommandlet.h"

#include "Commandlets/MeshGenerationCommandletUtils.h"
#include "SbxMeshGenLab/MeshGenerationRequest.h"
#include "SbxMeshGenLab/NativeMeshTypes.h"

UGenerateSandboxMeshHexTileCommandlet::UGenerateSandboxMeshHexTileCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshHexTileCommandlet::Main(FString const&) {
    auto const request{SandboxMesh::make_default_mesh_request(ESbxMeshShape::HexTile)};
    auto const asset_name{SandboxMesh::to_unreal_name(request.asset_name)};
    FVector const expected_extent{request.hex_tile.outer_radius,
                                  request.hex_tile.outer_radius * FMath::Sqrt(3.0) * 0.5,
                                  request.hex_tile.depth * 0.5};
    return SandboxMesh::commandlet_detail::generate_and_validate_mesh(
        asset_name,
        TEXT("hex tile"),
        expected_extent,
        0.01f,
        [&request] { return SandboxMesh::generate_mesh(request); },
        SandboxMesh::describe_mesh_request(request));
}
