#include "Commandlets/GenerateSandboxMeshHoneycombPanelCommandlet.h"

#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/MeshGenerationRequest.h"

#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"

UGenerateSandboxMeshHoneycombPanelCommandlet::UGenerateSandboxMeshHoneycombPanelCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshHoneycombPanelCommandlet::Main(FString const&) {
    auto const request{SandboxMesh::make_default_mesh_request(ESbxMeshShape::HoneycombPanel)};
    auto const mesh_data{SandboxMesh::generate_mesh(request)};
    auto* const static_mesh{SandboxMesh::write_generated_static_mesh_asset(
        mesh_data, request.asset_name, SandboxMesh::describe_mesh_request(request))};
    if (static_mesh == nullptr) {
        return 1;
    }

    auto const bounds{static_mesh->GetBounds()};
    if (bounds.Origin.ContainsNaN() || bounds.BoxExtent.ContainsNaN() ||
        !FMath::IsNearlyEqual(bounds.BoxExtent.Z, request.honeycomb_panel.depth * 0.5, 0.01)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Generated honeycomb panel has unexpected bounds: %s"),
               *bounds.BoxExtent.ToString());
        return 1;
    }

    auto const output_filename{SandboxMesh::get_generated_asset_filename(request.asset_name)};
    if (!IFileManager::Get().FileExists(*output_filename)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Generated honeycomb panel package does not exist: %s"),
               *output_filename);
        return 1;
    }

    return 0;
}
