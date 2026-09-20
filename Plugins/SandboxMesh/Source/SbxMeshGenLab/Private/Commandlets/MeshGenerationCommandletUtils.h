#pragma once

#include "Generation/MeshAssetWriter.h"

#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"

namespace SandboxMesh::commandlet_detail {
template <typename Generate>
auto generate_and_validate_mesh(FName const asset_name,
                                TCHAR const* const mesh_name,
                                FVector const expected_extent,
                                float const tolerance,
                                Generate&& generate,
                                FString const& generation_description = {}) -> int32 {
    auto const mesh_data{generate()};
    auto* const static_mesh{SandboxMesh::write_generated_static_mesh_asset(
        mesh_data, asset_name, generation_description)};
    if (static_mesh == nullptr) {
        return 1;
    }

    auto const bounds{static_mesh->GetBounds()};
    if (!bounds.BoxExtent.Equals(expected_extent, tolerance)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Generated %s has unexpected bounds: %s"),
               mesh_name,
               *bounds.BoxExtent.ToString());
        return 1;
    }

    auto const output_filename{SandboxMesh::get_generated_asset_filename(asset_name)};
    if (!IFileManager::Get().FileExists(*output_filename)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Generated %s package does not exist: %s"),
               mesh_name,
               *output_filename);
        return 1;
    }

    return 0;
}
}
