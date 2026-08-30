#include "Commandlets/GenerateSandboxMeshCubeCommandlet.h"

#include "Generation/MeshAssetWriter.h"

#include "HAL/FileManager.h"

UGenerateSandboxMeshCubeCommandlet::UGenerateSandboxMeshCubeCommandlet() {
    IsClient = false;
    IsEditor = true;
    LogToConsole = true;
    ShowErrorCount = false;
}

int32 UGenerateSandboxMeshCubeCommandlet::Main(FString const&) {
    auto* const static_mesh{SandboxMesh::generate_cube_asset()};
    if (static_mesh == nullptr) {
        return 1;
    }

    auto const output_filename{SandboxMesh::get_generated_cube_filename()};
    if (!IFileManager::Get().FileExists(*output_filename)) {
        UE_LOG(LogTemp, Error, TEXT("Generated cube package does not exist: %s"), *output_filename);
        return 1;
    }

    return 0;
}
