#include "Generation/MeshAssetWriter.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshOperations.h"
#include "UObject/MetaData.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

DEFINE_LOG_CATEGORY_STATIC(LogSbxMeshGenLab, Log, All);

namespace SandboxMesh {
namespace {
FString const generated_package_path{TEXT("/SandboxMesh/MeshGenLab/Generated")};

auto get_generated_asset_package_name(FName const asset_name) -> FString {
    return FString::Printf(TEXT("%s/%s"), *generated_package_path, *asset_name.ToString());
}

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

auto has_valid_bounds(FMeshDescription const& mesh_description) -> bool {
    auto const bounds{mesh_description.GetBounds()};
    return !bounds.Origin.ContainsNaN() && !bounds.BoxExtent.ContainsNaN() &&
           FMath::IsFinite(bounds.SphereRadius) && bounds.SphereRadius > 0.0;
}

auto has_valid_mesh_data(FSbxMeshData const& mesh_data) -> bool {
    if (mesh_data.positions.IsEmpty() || mesh_data.indices.IsEmpty() ||
        mesh_data.indices.Num() % 3 != 0 || mesh_data.normals.Num() != mesh_data.positions.Num() ||
        mesh_data.uvs.Num() != mesh_data.positions.Num()) {
        return false;
    }

    auto const vertex_count{static_cast<uint32>(mesh_data.positions.Num())};
    for (auto const index : mesh_data.indices) {
        if (index >= vertex_count) {
            return false;
        }
    }
    return true;
}

auto build_static_mesh(UStaticMesh& static_mesh,
                       FSbxMeshData const& mesh_data,
                       bool const fast_build) -> bool {
    if (!has_valid_mesh_data(mesh_data)) {
        UE_LOG(LogSbxMeshGenLab, Error, TEXT("Generated mesh buffers are invalid."));
        return false;
    }

    auto mesh_description{make_mesh_description(mesh_data)};
    FStaticMeshOperations::ComputeTriangleTangentsAndNormals(mesh_description);
    FStaticMeshOperations::ComputeTangentsAndNormals(mesh_description, EComputeNTBsFlags::Tangents);
    if (!has_valid_bounds(mesh_description)) {
        UE_LOG(LogSbxMeshGenLab, Error, TEXT("Generated mesh has invalid bounds."));
        return false;
    }

    TArray<FMeshDescription const*> mesh_descriptions{&mesh_description};
    static_mesh.Modify();
    static_mesh.PreEditChange(nullptr);
    static_mesh.GetStaticMaterials().Reset();
    static_mesh.GetStaticMaterials().Add(FStaticMaterial{});
    static_mesh.SetNumSourceModels(1);

    auto& build_settings{static_mesh.GetSourceModel(0).BuildSettings};
    build_settings.bRecomputeNormals = false;
    build_settings.bRecomputeTangents = false;
    build_settings.bGenerateLightmapUVs = false;

    UStaticMesh::FBuildMeshDescriptionsParams build_parameters{};
    build_parameters.bFastBuild = fast_build;
    static_mesh.BuildFromMeshDescriptions(mesh_descriptions, build_parameters);
    static_mesh.PostEditChange();
    return true;
}
}

auto get_generated_asset_filename(FName const asset_name) -> FString {
    return FPackageName::LongPackageNameToFilename(get_generated_asset_package_name(asset_name),
                                                   FPackageName::GetAssetPackageExtension());
}

auto get_generated_asset_object_path(FName const asset_name) -> FString {
    auto const package_name{get_generated_asset_package_name(asset_name)};
    return FString::Printf(TEXT("%s.%s"), *package_name, *asset_name.ToString());
}

auto create_transient_static_mesh(FSbxMeshData const& mesh_data) -> UStaticMesh* {
    auto* const static_mesh{NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient)};
    if (static_mesh == nullptr || !build_static_mesh(*static_mesh, mesh_data, true)) {
        return nullptr;
    }
    return static_mesh;
}

auto write_generated_static_mesh_asset(FSbxMeshData const& mesh_data,
                                       FName const asset_name,
                                       FString const& generation_description) -> UStaticMesh* {
    if (!ensure_generated_content_directory()) {
        return nullptr;
    }

    auto const package_name{get_generated_asset_package_name(asset_name)};
    auto const object_path{get_generated_asset_object_path(asset_name)};
    auto* static_mesh{LoadObject<UStaticMesh>(nullptr, *object_path)};
    auto const is_new_asset{static_mesh == nullptr};
    auto* const package{is_new_asset ? CreatePackage(*package_name) : static_mesh->GetOutermost()};
    if (package == nullptr) {
        UE_LOG(LogSbxMeshGenLab,
               Error,
               TEXT("Failed to create generated mesh package: %s"),
               *package_name);
        return nullptr;
    }

    if (is_new_asset) {
        static_mesh = NewObject<UStaticMesh>(
            package, asset_name, RF_Public | RF_Standalone | RF_Transactional);
    }
    if (static_mesh == nullptr) {
        UE_LOG(LogSbxMeshGenLab, Error, TEXT("Failed to create generated static mesh."));
        return nullptr;
    }

    if (!build_static_mesh(*static_mesh, mesh_data, false)) {
        UE_LOG(
            LogSbxMeshGenLab, Error, TEXT("Generated mesh could not be built: %s"), *object_path);
        return nullptr;
    }

    if (!generation_description.IsEmpty()) {
        package->GetMetaData().SetValue(
            static_mesh, TEXT("SandboxMesh.MeshGenLab.Generation"), *generation_description);
    }
    static_mesh->MarkPackageDirty();

    if (is_new_asset) {
        FAssetRegistryModule::AssetCreated(static_mesh);
    }

    FSavePackageArgs save_arguments{};
    save_arguments.TopLevelFlags = RF_Public | RF_Standalone;
    auto const package_filename{get_generated_asset_filename(asset_name)};
    if (!UPackage::SavePackage(package, static_mesh, *package_filename, save_arguments)) {
        UE_LOG(LogSbxMeshGenLab,
               Error,
               TEXT("Failed to save generated mesh asset: %s"),
               *package_filename);
        return nullptr;
    }

    UE_LOG(LogSbxMeshGenLab,
           Display,
           TEXT("Generated static mesh asset: %s"),
           *static_mesh->GetPathName());
    return static_mesh;
}

}
