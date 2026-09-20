#include "Commandlets/GenerateSandboxMeshSphereCommandlet.h"

#include "Commandlets/MeshGenerationCommandletUtils.h"
#include "SbxMeshGenLab/SphereGenerator.h"

UGenerateSandboxMeshSphereCommandlet::UGenerateSandboxMeshSphereCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshSphereCommandlet::Main(FString const&) {
    FName const asset_name{TEXT("SM_GeneratedSphere")};
    return SandboxMesh::commandlet_detail::generate_and_validate_mesh(
        asset_name, TEXT("sphere"), FVector{50.0, 50.0, 50.0}, 0.0f, [] {
            return SandboxMesh::generate_sphere(FSbxSphereParameters{});
        });
}
