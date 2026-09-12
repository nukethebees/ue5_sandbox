#include "SandboxEditor/Commandlets/GenerateWorldSoftTargetAssetsCommandlet.h"

#include <SbxMeshGenLab/HexFrameGenerator.h>
#include <SbxMeshGenLab/MeshAssetWriter.h>
#include "SpaceGame/simulation/SpaceGameLevelConfig.h"

#include <Engine/StaticMesh.h>
#include <Materials/MaterialInterface.h>
#include <Misc/PackageName.h>
#include <UObject/SavePackage.h>

namespace ml::world_soft_target_assets {
inline constexpr TCHAR material_object_path[]{
    TEXT("/SpaceGame/Generated/Materials/M_SoftTargetWorld.M_SoftTargetWorld")};
inline constexpr TCHAR mesh_object_path[]{
    TEXT("/SpaceGame/UI/InGame/WorldMarkers/SM_SoftTargetHex.SM_SoftTargetHex")};
inline constexpr TCHAR source_config_object_path[]{
    TEXT("/Game/Levels/FeatureTests/FT_soa_turrets/DA_FT_soa_entities_LevelConfig."
         "DA_FT_soa_entities_LevelConfig")};
inline constexpr TCHAR runtime_config_object_path[]{
    TEXT("/SpaceGame/Levels/DA_GameRuntimeLevelConfig.DA_GameRuntimeLevelConfig")};

auto save_asset(UObject& asset) -> bool {
    auto* const package{asset.GetOutermost()};
    package->MarkPackageDirty();
    auto const filename{FPackageName::LongPackageNameToFilename(
        package->GetName(), FPackageName::GetAssetPackageExtension())};
    FSavePackageArgs arguments{};
    arguments.TopLevelFlags = RF_Public | RF_Standalone;
    return UPackage::SavePackage(package, &asset, *filename, arguments);
}

auto configure_level_config(TCHAR const* const object_path,
                            UStaticMesh& mesh,
                            UMaterialInterface& material) -> bool {
    auto* const config{LoadObject<USpaceGameLevelConfig>(nullptr, object_path)};
    if (!IsValid(config)) {
        UE_LOG(LogTemp, Error, TEXT("Could not load world soft-target config: %s"), object_path);
        return false;
    }
    config->Modify();
    config->entity_overlay.soft_target.mesh = &mesh;
    config->entity_overlay.soft_target.material = &material;
    return save_asset(*config);
}
}

UGenerateWorldSoftTargetAssetsCommandlet::UGenerateWorldSoftTargetAssetsCommandlet() {
    IsClient = true;
    IsEditor = true;
    LogToConsole = true;
}

int32 UGenerateWorldSoftTargetAssetsCommandlet::Main(FString const&) {
    using namespace ml::world_soft_target_assets;

    auto* const material{LoadObject<UMaterialInterface>(nullptr, material_object_path)};
    if (!IsValid(material)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Could not load generated world soft-target material: %s"),
               material_object_path);
        return 1;
    }
    auto const mesh_data{SandboxMesh::generate_hex_frame(
        {.outer_radius = 50.0f, .wall_thickness = 5.0f, .depth = 2.0f, .pointy_top = false})};
    auto* const mesh{SandboxMesh::write_static_mesh_asset(
        mesh_data,
        mesh_object_path,
        *material,
        TEXT("MeshGenLab flat-top hex frame: outer_radius=50, wall_thickness=5, depth=2"))};
    if (!IsValid(mesh)) {
        return 1;
    }

    auto success{configure_level_config(source_config_object_path, *mesh, *material)};
    success &= configure_level_config(runtime_config_object_path, *mesh, *material);
    return success ? 0 : 1;
}
