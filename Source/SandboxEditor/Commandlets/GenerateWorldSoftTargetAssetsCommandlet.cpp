#include "SandboxEditor/Commandlets/GenerateWorldSoftTargetAssetsCommandlet.h"

#include "SpaceGame/simulation/SpaceGameLevelConfig.h"
#include "SpaceGamePresentation/presentation/WorldSoftTargetMaterialData.h"

#include <SbxMeshGenLab/HexFrameGenerator.h>
#include <SbxMeshGenLab/MeshAssetWriter.h>

#include <AssetRegistry/AssetRegistryModule.h>
#include <Engine/StaticMesh.h>
#include <MaterialDomain.h>
#include <MaterialEditingLibrary.h>
#include <Materials/Material.h>
#include <Materials/MaterialExpressionCustom.h>
#include <Materials/MaterialExpressionPerInstanceCustomData.h>
#include <Misc/PackageName.h>
#include <ShaderCompiler.h>
#include <UObject/Package.h>
#include <UObject/SavePackage.h>

namespace ml::world_soft_target_assets {
inline constexpr TCHAR material_package_name[]{
    TEXT("/SpaceGame/UI/InGame/WorldMarkers/M_SoftTargetWorld")};
inline constexpr TCHAR material_asset_name[]{TEXT("M_SoftTargetWorld")};
inline constexpr TCHAR material_object_path[]{
    TEXT("/SpaceGame/UI/InGame/WorldMarkers/M_SoftTargetWorld.M_SoftTargetWorld")};
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

auto make_custom_data(UMaterial& material, int32 const data_index, int32 const editor_y)
    -> UMaterialExpressionPerInstanceCustomData* {
    auto* const expression{CastChecked<UMaterialExpressionPerInstanceCustomData>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            &material, UMaterialExpressionPerInstanceCustomData::StaticClass(), -500, editor_y))};
    expression->DataIndex = data_index;
    return expression;
}

auto generate_material() -> UMaterial* {
    auto* const package{CreatePackage(material_package_name)};
    package->FullyLoad();
    auto* material{FindObject<UMaterial>(package, material_asset_name)};
    auto const is_new_asset{material == nullptr};
    if (is_new_asset) {
        material = NewObject<UMaterial>(
            package, material_asset_name, RF_Public | RF_Standalone | RF_Transactional);
    }
    if (material == nullptr) {
        return nullptr;
    }

    UMaterialEditingLibrary::DeleteAllMaterialExpressions(material);
    material->MaterialDomain = MD_Surface;
    material->BlendMode = BLEND_Translucent;
    material->SetShadingModel(MSM_Unlit);
    material->TwoSided = true;
    material->bDisableDepthTest = true;
    material->SetMaterialUsage(MATUSAGE_InstancedStaticMeshes);

    auto* const red{make_custom_data(*material, ml::soft_target_world::color_red_index, -300)};
    auto* const green{make_custom_data(*material, ml::soft_target_world::color_green_index, -200)};
    auto* const blue{make_custom_data(*material, ml::soft_target_world::color_blue_index, -100)};
    auto* const opacity{make_custom_data(*material, ml::soft_target_world::opacity_index, 100)};
    auto* const intensity{make_custom_data(*material, ml::soft_target_world::intensity_index, 200)};
    auto* const range_alpha{
        make_custom_data(*material, ml::soft_target_world::range_alpha_index, 300)};
    auto* const emissive{
        CastChecked<UMaterialExpressionCustom>(UMaterialEditingLibrary::CreateMaterialExpression(
            material, UMaterialExpressionCustom::StaticClass(), -100, -150))};
    emissive->OutputType = CMOT_Float3;
    emissive->Description = TEXT("Per-instance soft-target colour, intensity, and range alpha");
    emissive->Code = TEXT("float ClampedRangeAlpha = saturate(RangeAlpha);\n"
                          "float RangeIntensity = lerp(1.0, 2.0, ClampedRangeAlpha);\n"
                          "return float3(Red, Green, Blue) * Intensity * RangeIntensity * 2.0;");
    for (auto const input : {TPair<FName, UMaterialExpression*>{TEXT("Red"), red},
                             {TEXT("Green"), green},
                             {TEXT("Blue"), blue},
                             {TEXT("Intensity"), intensity},
                             {TEXT("RangeAlpha"), range_alpha}}) {
        auto& material_input{emissive->Inputs.AddDefaulted_GetRef()};
        material_input.InputName = input.Key;
        material_input.Input.Connect(0, input.Value);
    }
    UMaterialEditingLibrary::ConnectMaterialProperty(emissive, TEXT(""), MP_EmissiveColor);
    UMaterialEditingLibrary::ConnectMaterialProperty(opacity, TEXT(""), MP_Opacity);

    material->PostEditChange();
    auto const errors{UMaterialEditingLibrary::RecompileMaterial(material)};
    if (!errors.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("World soft-target material compilation failed."));
        return nullptr;
    }
    GShaderCompilingManager->FinishAllCompilation();
    if (is_new_asset) {
        FAssetRegistryModule::AssetCreated(material);
    }
    return save_asset(*material) ? material : nullptr;
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

    auto* const material{generate_material()};
    if (!IsValid(material)) {
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
