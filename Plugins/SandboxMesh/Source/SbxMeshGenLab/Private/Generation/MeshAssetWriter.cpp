#include "Generation/MeshAssetWriter.h"

#include "SbxMeshGenLab/CubeGenerator.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogSbxMeshGenLab, Log, All);

namespace SandboxMesh {
namespace {
FString const generated_cube_package_name{
    TEXT("/SandboxMesh/MeshGenLab/Generated/SM_GeneratedCube")};
FName const generated_cube_asset_name{TEXT("SM_GeneratedCube")};
FString const generated_cube_object_path{
    TEXT("/SandboxMesh/MeshGenLab/Generated/SM_GeneratedCube.SM_GeneratedCube")};

auto ensure_generated_content_directory() -> bool {
    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxMesh"))};
    if (!plugin.IsValid()) {
        UE_LOG(LogSbxMeshGenLab, Error, TEXT("SandboxMesh plugin was not found."));
        return false;
    }

    auto const output_directory{
        FPaths::Combine(plugin->GetContentDir(), TEXT("MeshGenLab"), TEXT("Generated"))};
    auto& file_manager{IFileManager::Get()};
    if (!file_manager.MakeDirectory(*output_directory, true) &&
        !file_manager.DirectoryExists(*output_directory)) {
        UE_LOG(LogSbxMeshGenLab,
               Error,
               TEXT("Failed to create the generated mesh output directory: %s"),
               *output_directory);
        return false;
    }

    return true;
}

auto make_mesh_description(FSbxMeshData const& mesh_data) -> FMeshDescription {
    FMeshDescription mesh_description{};
    FStaticMeshAttributes attributes{mesh_description};
    attributes.Register();

    auto vertex_positions{attributes.GetVertexPositions()};
    auto vertex_instance_normals{attributes.GetVertexInstanceNormals()};
    auto vertex_instance_uvs{attributes.GetVertexInstanceUVs()};
    vertex_instance_uvs.SetNumChannels(1);

    TArray<FVertexID> vertices;
    vertices.Reserve(mesh_data.positions.Num());
    for (auto const position : mesh_data.positions) {
        auto const vertex_id{mesh_description.CreateVertex()};
        vertex_positions[vertex_id] = position;
        vertices.Add(vertex_id);
    }

    auto const polygon_group{mesh_description.CreatePolygonGroup()};
    auto const triangle_count{mesh_data.indices.Num() / 3};
    for (int32 triangle_index{0}; triangle_index < triangle_count; ++triangle_index) {
        TArray<FVertexInstanceID> vertex_instances;
        vertex_instances.Reserve(3);

        for (int32 corner_index{0}; corner_index < 3; ++corner_index) {
            auto const mesh_index{mesh_data.indices[triangle_index * 3 + corner_index]};
            auto const vertex_instance{mesh_description.CreateVertexInstance(vertices[mesh_index])};
            vertex_instance_normals[vertex_instance] = mesh_data.normals[mesh_index];
            vertex_instance_uvs.Set(vertex_instance, 0, mesh_data.uvs[mesh_index]);
            vertex_instances.Add(vertex_instance);
        }

        mesh_description.CreatePolygon(polygon_group, vertex_instances);
    }

    return mesh_description;
}
}

auto get_generated_cube_filename() -> FString {
    return FPackageName::LongPackageNameToFilename(generated_cube_package_name,
                                                   FPackageName::GetAssetPackageExtension());
}

auto generate_cube_asset() -> UStaticMesh* {
    if (!ensure_generated_content_directory()) {
        return nullptr;
    }

    auto* static_mesh{LoadObject<UStaticMesh>(nullptr, *generated_cube_object_path)};
    auto const is_new_asset{static_mesh == nullptr};
    auto* const package{is_new_asset ? CreatePackage(*generated_cube_package_name)
                                     : static_mesh->GetOutermost()};
    if (package == nullptr) {
        UE_LOG(LogSbxMeshGenLab,
               Error,
               TEXT("Failed to create generated cube package: %s"),
               *generated_cube_package_name);
        return nullptr;
    }

    if (is_new_asset) {
        static_mesh = NewObject<UStaticMesh>(
            package, generated_cube_asset_name, RF_Public | RF_Standalone | RF_Transactional);
    }
    if (static_mesh == nullptr) {
        UE_LOG(LogSbxMeshGenLab, Error, TEXT("Failed to create generated cube static mesh."));
        return nullptr;
    }

    auto const mesh_data{generate_cube()};
    auto mesh_description{make_mesh_description(mesh_data)};
    TArray<FMeshDescription const*> mesh_descriptions{&mesh_description};

    static_mesh->Modify();
    static_mesh->GetStaticMaterials().Reset();
    static_mesh->GetStaticMaterials().Add(FStaticMaterial{});

    UStaticMesh::FBuildMeshDescriptionsParams build_parameters{};
    build_parameters.bFastBuild = true;
    static_mesh->BuildFromMeshDescriptions(mesh_descriptions, build_parameters);
    static_mesh->MarkPackageDirty();

    if (is_new_asset) {
        FAssetRegistryModule::AssetCreated(static_mesh);
    }

    FSavePackageArgs save_arguments{};
    save_arguments.TopLevelFlags = RF_Public | RF_Standalone;
    auto const package_filename{get_generated_cube_filename()};
    if (!UPackage::SavePackage(package, static_mesh, *package_filename, save_arguments)) {
        UE_LOG(LogSbxMeshGenLab,
               Error,
               TEXT("Failed to save generated cube asset: %s"),
               *package_filename);
        return nullptr;
    }

    UE_LOG(LogSbxMeshGenLab,
           Display,
           TEXT("Generated cube static mesh asset: %s"),
           *static_mesh->GetPathName());
    return static_mesh;
}

}
