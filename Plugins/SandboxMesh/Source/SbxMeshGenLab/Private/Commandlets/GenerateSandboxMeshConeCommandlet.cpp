#include "Commandlets/GenerateSandboxMeshConeCommandlet.h"

#include "SbxMeshGenLab/ConeGenerator.h"

#include "Commandlets/MeshGenerationCommandletUtils.h"

UGenerateSandboxMeshConeCommandlet::UGenerateSandboxMeshConeCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshConeCommandlet::Main(FString const&) {
    FName const asset_name{TEXT("SM_GeneratedCone")};
    return SandboxMesh::commandlet_detail::generate_and_validate_mesh(
        asset_name, TEXT("cone"), FVector{50.0, 50.0, 50.0}, 0.0f, [] {
            return SandboxMesh::generate_cone(FSbxConeParameters{});
        });
}
