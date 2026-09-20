#include "Commandlets/GenerateSandboxMeshBoxCommandlet.h"

#include "Commandlets/MeshGenerationCommandletUtils.h"
#include "SbxMeshGenLab/BoxGenerator.h"

UGenerateSandboxMeshBoxCommandlet::UGenerateSandboxMeshBoxCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshBoxCommandlet::Main(FString const&) {
    FName const asset_name{TEXT("SM_GeneratedBox")};
    return SandboxMesh::commandlet_detail::generate_and_validate_mesh(
        asset_name, TEXT("box"), FVector{50.0, 50.0, 50.0}, 0.0f, [] {
            return SandboxMesh::generate_box(FSbxBoxParameters{});
        });
}
