#include "Commandlets/GenerateSandboxMeshCylinderCommandlet.h"

#include "Commandlets/MeshGenerationCommandletUtils.h"
#include "SbxMeshGenLab/CylinderGenerator.h"

UGenerateSandboxMeshCylinderCommandlet::UGenerateSandboxMeshCylinderCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshCylinderCommandlet::Main(FString const&) {
    FName const asset_name{TEXT("SM_GeneratedCylinder")};
    return SandboxMesh::commandlet_detail::generate_and_validate_mesh(
        asset_name, TEXT("cylinder"), FVector{50.0, 50.0, 50.0}, 0.0f, [] {
            return SandboxMesh::generate_cylinder(FSbxCylinderParameters{});
        });
}
