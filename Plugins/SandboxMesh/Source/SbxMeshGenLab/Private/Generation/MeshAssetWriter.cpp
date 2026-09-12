#include "Generation/MeshAssetWriter.h"
#include "SbxMeshGenLab/MeshMaterialRole.h"
#include "SbxMeshGenLab/NativeMeshTypes.h"

#include <cstddef>

#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInstanceDynamic.h"
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

auto get_generated_package_path(FString const& relative_directory) -> FString {
    return relative_directory.IsEmpty()
             ? generated_package_path
             : FString::Printf(TEXT("%s/%s"), *generated_package_path, *relative_directory);
}

auto get_generated_asset_package_name(FName const asset_name, FString const& relative_directory)
    -> FString {
    return FString::Printf(
        TEXT("%s/%s"), *get_generated_package_path(relative_directory), *asset_name.ToString());
}

auto is_valid_relative_directory(FString const& relative_directory) -> bool {
    if (relative_directory.IsEmpty()) {
        return true;
    }

    return FPaths::IsRelative(relative_directory) && !relative_directory.Contains(TEXT("..")) &&
           !relative_directory.StartsWith(TEXT("/")) &&
           !relative_directory.StartsWith(TEXT("\\")) &&
           FPackageName::IsValidLongPackageName(get_generated_package_path(relative_directory));
}

auto ensure_generated_content_directory(FString const& relative_directory) -> bool {
    if (!is_valid_relative_directory(relative_directory)) {
        UE_LOG(LogSbxMeshGenLab,
               Error,
               TEXT("Invalid generated mesh subdirectory: %s"),
               *relative_directory);
        return false;
    }

    auto const plugin{IPluginManager::Get().FindPlugin(TEXT("SandboxMesh"))};
    if (!plugin.IsValid()) {
        UE_LOG(LogSbxMeshGenLab, Error, TEXT("SandboxMesh plugin was not found."));
        return false;
    }

    auto const output_directory{FPaths::Combine(
        plugin->GetContentDir(), TEXT("MeshGenLab"), TEXT("Generated"), relative_directory)};
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

auto get_role_material_asset_name(ESbxMeshMaterialRole const role, FName const unique_suffix)
    -> FName {
    auto const role_name{get_mesh_material_slot_name(role).ToString()};
    return unique_suffix.IsNone()
             ? FName{FString::Printf(TEXT("MI_Sbx%s"), *role_name)}
             : FName{FString::Printf(TEXT("MI_Sbx%s_%s"), *role_name, *unique_suffix.ToString())};
}

auto get_basic_shape_material() -> UMaterialInterface* {
    auto* const material{LoadObject<UMaterialInterface>(
        nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"))};
    if (material == nullptr) {
        UE_LOG(LogSbxMeshGenLab, Error, TEXT("Failed to load Unreal's Basic Shape material."));
    }
    return material;
}

auto create_transient_role_material(UObject& outer, ESbxMeshMaterialRole const role)
    -> UMaterialInterface* {
    auto* const parent{get_basic_shape_material()};
    if (parent == nullptr) {
        return nullptr;
    }

    auto* const material{UMaterialInstanceDynamic::Create(parent, &outer)};
    material->SetVectorParameterValue(TEXT("Color"), get_mesh_material_color(role));
    return material;
}

auto load_or_create_role_material(ESbxMeshMaterialRole const role,
                                  FString const& package_path,
                                  FName const unique_suffix) -> UMaterialInterface* {
    auto* const parent{get_basic_shape_material()};
    if (parent == nullptr) {
        return nullptr;
    }

    auto const asset_name{get_role_material_asset_name(role, unique_suffix)};
    auto const package_name{FString::Printf(TEXT("%s/%s"), *package_path, *asset_name.ToString())};
    auto const object_path{FString::Printf(TEXT("%s.%s"), *package_name, *asset_name.ToString())};
    auto* material{
        LoadObject<UMaterialInstanceConstant>(nullptr, *object_path, nullptr, LOAD_NoWarn)};
    auto const is_new_asset{material == nullptr};
    auto* const package{is_new_asset ? CreatePackage(*package_name) : material->GetOutermost()};
    if (package == nullptr) {
        UE_LOG(LogSbxMeshGenLab,
               Error,
               TEXT("Failed to create generated material package: %s"),
               *package_name);
        return nullptr;
    }

    if (is_new_asset) {
        material = NewObject<UMaterialInstanceConstant>(
            package, asset_name, RF_Public | RF_Standalone | RF_Transactional);
    }
    if (material == nullptr) {
        return nullptr;
    }

    material->SetParentEditorOnly(parent);
    material->SetVectorParameterValueEditorOnly(FMaterialParameterInfo{TEXT("Color")},
                                                get_mesh_material_color(role));
    material->PostEditChange();
    material->MarkPackageDirty();
    if (is_new_asset) {
        FAssetRegistryModule::AssetCreated(material);
    }

    FSavePackageArgs save_arguments{};
    save_arguments.TopLevelFlags = RF_Public | RF_Standalone;
    auto const package_filename{FPackageName::LongPackageNameToFilename(
        package_name, FPackageName::GetAssetPackageExtension())};
    if (!UPackage::SavePackage(package, material, *package_filename, save_arguments)) {
        UE_LOG(LogSbxMeshGenLab,
               Error,
               TEXT("Failed to save generated role material: %s"),
               *package_filename);
        return nullptr;
    }
    return material;
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
    vertices.Reserve(static_cast<int32>(mesh_data.positions.size()));
    for (auto const position : mesh_data.positions) {
        auto const vertex_id{mesh_description.CreateVertex()};
        vertex_positions[vertex_id] = SandboxMesh::to_unreal_float(position);
        vertices.Add(vertex_id);
    }

    auto material_slot_names{attributes.GetPolygonGroupMaterialSlotNames()};
    TArray<FPolygonGroupID> polygon_groups;
    polygon_groups.Reserve(mesh_material_role_count);
    for (int32 role_index{}; role_index < mesh_material_role_count; ++role_index) {
        auto const role{static_cast<ESbxMeshMaterialRole>(role_index)};
        auto const polygon_group{mesh_description.CreatePolygonGroup()};
        material_slot_names[polygon_group] = get_mesh_material_slot_name(role);
        polygon_groups.Add(polygon_group);
    }

    auto const triangle_count{mesh_data.indices.size() / 3};
    for (std::size_t triangle_index{}; triangle_index < triangle_count; ++triangle_index) {
        TArray<FVertexInstanceID> vertex_instances;
        vertex_instances.Reserve(3);

        for (int32 corner_index{0}; corner_index < 3; ++corner_index) {
            auto const mesh_index{mesh_data.indices[triangle_index * 3 + corner_index]};
            auto const vertex_instance{mesh_description.CreateVertexInstance(vertices[mesh_index])};
            vertex_instance_normals[vertex_instance] =
                SandboxMesh::to_unreal_float(mesh_data.normals[mesh_index]);
            vertex_instance_uvs.Set(
                vertex_instance, 0, SandboxMesh::to_unreal(mesh_data.uvs[mesh_index]));
            vertex_instances.Add(vertex_instance);
        }

        auto const role{
            mesh_data.triangle_material_roles.empty()
                ? ESbxMeshMaterialRole::Structure
                : SandboxMesh::to_unreal(mesh_data.triangle_material_roles[triangle_index])};
        auto const role_index{static_cast<int32>(role)};
        check(polygon_groups.IsValidIndex(role_index));
        mesh_description.CreatePolygon(polygon_groups[role_index], vertex_instances);
    }

    return mesh_description;
}

auto has_valid_bounds(FMeshDescription const& mesh_description) -> bool {
    auto const bounds{mesh_description.GetBounds()};
    return !bounds.Origin.ContainsNaN() && !bounds.BoxExtent.ContainsNaN() &&
           FMath::IsFinite(bounds.SphereRadius) && bounds.SphereRadius > 0.0;
}

auto build_static_mesh(UStaticMesh& static_mesh,
                       FSbxMeshData const& mesh_data,
                       bool const fast_build,
                       bool const persistent_materials,
                       FString const& material_package_path = {},
                       FName const material_unique_suffix = NAME_None) -> bool {
    if (!mesh_gen::is_valid_mesh_data(mesh_data)) {
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
    for (int32 role_index{}; role_index < mesh_material_role_count; ++role_index) {
        auto const role{static_cast<ESbxMeshMaterialRole>(role_index)};
        auto* const material{
            persistent_materials
                ? load_or_create_role_material(role, material_package_path, material_unique_suffix)
                : create_transient_role_material(static_mesh, role)};
        auto const slot_name{get_mesh_material_slot_name(role)};
        static_mesh.GetStaticMaterials().Add(FStaticMaterial{material, slot_name, slot_name});
    }
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

auto get_generated_asset_filename(FName const asset_name, FString const& relative_directory)
    -> FString {
    return FPackageName::LongPackageNameToFilename(
        get_generated_asset_package_name(asset_name, relative_directory),
        FPackageName::GetAssetPackageExtension());
}

auto get_generated_asset_object_path(FName const asset_name, FString const& relative_directory)
    -> FString {
    auto const package_name{get_generated_asset_package_name(asset_name, relative_directory)};
    return FString::Printf(TEXT("%s.%s"), *package_name, *asset_name.ToString());
}

auto create_transient_static_mesh(FSbxMeshData const& mesh_data) -> UStaticMesh* {
    auto* const static_mesh{NewObject<UStaticMesh>(GetTransientPackage(), NAME_None, RF_Transient)};
    if (static_mesh == nullptr || !build_static_mesh(*static_mesh, mesh_data, true, false)) {
        return nullptr;
    }
    return static_mesh;
}

auto write_generated_static_mesh_asset(FSbxMeshData const& mesh_data,
                                       FName const asset_name,
                                       FString const& generation_description,
                                       FString const& relative_directory) -> UStaticMesh* {
    if (!ensure_generated_content_directory(relative_directory)) {
        return nullptr;
    }

    auto const package_path{get_generated_package_path(relative_directory)};
    auto const package_name{get_generated_asset_package_name(asset_name, relative_directory)};
    auto const object_path{get_generated_asset_object_path(asset_name, relative_directory)};
    FText invalid_name_reason;
    if (asset_name.IsNone() ||
        !FPackageName::IsValidObjectPath(object_path, &invalid_name_reason)) {
        UE_LOG(LogSbxMeshGenLab,
               Error,
               TEXT("Invalid generated mesh asset name '%s': %s"),
               *asset_name.ToString(),
               *invalid_name_reason.ToString());
        return nullptr;
    }
    auto* static_mesh{LoadObject<UStaticMesh>(nullptr, *object_path, nullptr, LOAD_NoWarn)};
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

    auto const material_unique_suffix{relative_directory.IsEmpty() ? NAME_None : asset_name};
    if (!build_static_mesh(
            *static_mesh, mesh_data, false, true, package_path, material_unique_suffix)) {
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
    auto const package_filename{get_generated_asset_filename(asset_name, relative_directory)};
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
