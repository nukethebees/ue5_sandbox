#include "Commandlets/GenerateSandboxMeshHexFrameCommandlet.h"

#include "SbxMeshGenLab/HexFrameGenerator.h"

#include "Commandlets/MeshGenerationCommandletUtils.h"

UGenerateSandboxMeshHexFrameCommandlet::UGenerateSandboxMeshHexFrameCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshHexFrameCommandlet::Main(FString const&) {
    FName const asset_name{TEXT("SM_GeneratedHexFrame")};
    FVector const expected_extent{50.0, 25.0 * FMath::Sqrt(3.0), 10.0};
    return SandboxMesh::commandlet_detail::generate_and_validate_mesh(
        asset_name, TEXT("hex frame"), expected_extent, 0.01f, [] {
            return SandboxMesh::generate_hex_frame(FSbxHexFrameParameters{});
        });
}
