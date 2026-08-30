#include "Commandlets/GenerateSandboxMeshHexFrameCommandlet.h"

#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/HexFrameGenerator.h"

#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"

UGenerateSandboxMeshHexFrameCommandlet::UGenerateSandboxMeshHexFrameCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshHexFrameCommandlet::Main(FString const&) {
    FName const asset_name{TEXT("SM_GeneratedHexFrame")};
    auto const mesh_data{SandboxMesh::generate_hex_frame()};
    auto* const static_mesh{SandboxMesh::write_generated_static_mesh_asset(mesh_data, asset_name)};
    if (static_mesh == nullptr) {
        return 1;
    }

    auto const bounds{static_mesh->GetBounds()};
    FVector const expected_extent{50.0, 25.0 * FMath::Sqrt(3.0), 10.0};
    if (!bounds.BoxExtent.Equals(expected_extent, 0.01)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Generated hex frame has unexpected bounds: %s"),
               *bounds.BoxExtent.ToString());
        return 1;
    }

    auto const output_filename{SandboxMesh::get_generated_asset_filename(asset_name)};
    if (!IFileManager::Get().FileExists(*output_filename)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Generated hex frame package does not exist: %s"),
               *output_filename);
        return 1;
    }

    return 0;
}
