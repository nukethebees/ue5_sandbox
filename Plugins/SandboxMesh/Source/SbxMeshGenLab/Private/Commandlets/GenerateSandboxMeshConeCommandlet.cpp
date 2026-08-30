#include "Commandlets/GenerateSandboxMeshConeCommandlet.h"

#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/ConeGenerator.h"

#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"

UGenerateSandboxMeshConeCommandlet::UGenerateSandboxMeshConeCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshConeCommandlet::Main(FString const&) {
    FName const asset_name{TEXT("SM_GeneratedCone")};
    auto const mesh_data{SandboxMesh::generate_cone()};
    auto* const static_mesh{SandboxMesh::write_generated_static_mesh_asset(mesh_data, asset_name)};
    if (static_mesh == nullptr) {
        return 1;
    }

    auto const bounds{static_mesh->GetBounds()};
    if (!bounds.BoxExtent.Equals(FVector{50.0, 50.0, 50.0})) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Generated cone has unexpected bounds: %s"),
               *bounds.BoxExtent.ToString());
        return 1;
    }

    auto const output_filename{SandboxMesh::get_generated_asset_filename(asset_name)};
    if (!IFileManager::Get().FileExists(*output_filename)) {
        UE_LOG(LogTemp, Error, TEXT("Generated cone package does not exist: %s"), *output_filename);
        return 1;
    }

    return 0;
}
