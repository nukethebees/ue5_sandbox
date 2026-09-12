#include "Commandlets/GenerateSandboxMeshHexTileCommandlet.h"

#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/MeshGenerationRequest.h"
#include "SbxMeshGenLab/NativeMeshTypes.h"

#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"

UGenerateSandboxMeshHexTileCommandlet::UGenerateSandboxMeshHexTileCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshHexTileCommandlet::Main(FString const&) {
    auto const request{SandboxMesh::make_default_mesh_request(ESbxMeshShape::HexTile)};
    auto const mesh_data{SandboxMesh::generate_mesh(request)};
    auto const asset_name{SandboxMesh::to_unreal_name(request.asset_name)};
    auto* const static_mesh{SandboxMesh::write_generated_static_mesh_asset(
        mesh_data, asset_name, SandboxMesh::describe_mesh_request(request))};
    if (static_mesh == nullptr) {
        return 1;
    }

    auto const bounds{static_mesh->GetBounds()};
    FVector const expected_extent{request.hex_tile.outer_radius,
                                  request.hex_tile.outer_radius * FMath::Sqrt(3.0) * 0.5,
                                  request.hex_tile.depth * 0.5};
    if (!bounds.BoxExtent.Equals(expected_extent, 0.01)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Generated hex tile has unexpected bounds: %s"),
               *bounds.BoxExtent.ToString());
        return 1;
    }

    auto const output_filename{SandboxMesh::get_generated_asset_filename(asset_name)};
    if (!IFileManager::Get().FileExists(*output_filename)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Generated hex tile package does not exist: %s"),
               *output_filename);
        return 1;
    }

    return 0;
}
