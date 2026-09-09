#include "SandboxEditor/slate/UiGlowLab.h"

#include "SandboxUI/EntityOverlay/SEntityOverlayWidget.h"

#include "Engine/Texture2D.h"
#include "MaterialDomain.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/PackageName.h"
#include "ShaderCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace ml::ui::glow_lab {
auto generate_material() -> bool {
    auto* const texture_package{CreatePackage(TEXT("/SandboxUI/Materials/T_UiGlowBlack"))};
    texture_package->FullyLoad();
    auto* texture{FindObject<UTexture2D>(texture_package, TEXT("T_UiGlowBlack"))};
    if (texture == nullptr) {
        texture = NewObject<UTexture2D>(
            texture_package, TEXT("T_UiGlowBlack"), RF_Public | RF_Standalone);
    }
    uint8 const black[]{0, 0, 0, 255};
    texture->Source.Init(1, 1, 1, 1, TSF_BGRA8, black);
    texture->SRGB = false;
    texture->CompressionSettings = TC_VectorDisplacementmap;
    texture->MipGenSettings = TMGS_NoMipmaps;
    texture->PostEditChange();

    auto const package_name{
        FPackageName::ObjectPathToPackageName(FString{SEntityOverlayWidget::glow_material_path})};
    auto* const package{CreatePackage(*package_name)};
    package->FullyLoad();
    auto* material{FindObject<UMaterial>(package, TEXT("M_UiGlowComposite"))};
    if (material == nullptr) {
        material =
            NewObject<UMaterial>(package, TEXT("M_UiGlowComposite"), RF_Public | RF_Standalone);
    }
    UMaterialEditingLibrary::DeleteAllMaterialExpressions(material);
    material->MaterialDomain = MD_UI;
    material->BlendMode = BLEND_Additive;
    auto* const energy{CastChecked<UMaterialExpressionTextureObjectParameter>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            material, UMaterialExpressionTextureObjectParameter::StaticClass(), -600, -200))};
    energy->ParameterName = TEXT("GlowEnergy");
    energy->Texture = texture;
    energy->SamplerType = SAMPLERTYPE_LinearColor;
    auto* const core{CastChecked<UMaterialExpressionTextureObjectParameter>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            material, UMaterialExpressionTextureObjectParameter::StaticClass(), -600, 0))};
    core->ParameterName = TEXT("CoreTexture");
    core->Texture = texture;
    core->SamplerType = SAMPLERTYPE_LinearColor;
    auto* const uv{UMaterialEditingLibrary::CreateMaterialExpression(
        material, UMaterialExpressionTextureCoordinate::StaticClass(), -600, 200)};
    auto* const color{CastChecked<UMaterialExpressionVectorParameter>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            material, UMaterialExpressionVectorParameter::StaticClass(), -600, 400))};
    color->ParameterName = TEXT("GlowColor");
    color->DefaultValue = FLinearColor{0.84f, 0.65f, 0.23f, 1.0f};
    auto* const protection{CastChecked<UMaterialExpressionScalarParameter>(
        UMaterialEditingLibrary::CreateMaterialExpression(
            material, UMaterialExpressionScalarParameter::StaticClass(), -600, 600))};
    protection->ParameterName = TEXT("PreserveCorePixels");
    protection->DefaultValue = 0.0f;
    auto* const composite{
        CastChecked<UMaterialExpressionCustom>(UMaterialEditingLibrary::CreateMaterialExpression(
            material, UMaterialExpressionCustom::StaticClass(), -200, 0))};
    composite->OutputType = CMOT_Float3;
    composite->Description = TEXT("Additive glow with optional strict core-pixel preservation");
    composite->Code =
        TEXT("float core_alpha = Texture2DSample(Core, CoreSampler, UV).a;\n"
             "float energy = Texture2DSample(Energy, EnergySampler, UV).r;\n"
             "return Preserve > 0.5 && core_alpha > 0.0 ? float3(0, 0, 0) : energy * Color.rgb;\n");
    composite->Inputs.Empty();
    auto add_input{[composite](FName const name, UMaterialExpression* const expression) {
        auto& input{composite->Inputs.AddDefaulted_GetRef()};
        input.InputName = name;
        input.Input.Connect(0, expression);
    }};
    add_input(TEXT("Energy"), energy);
    add_input(TEXT("Core"), core);
    add_input(TEXT("UV"), uv);
    add_input(TEXT("Color"), color);
    add_input(TEXT("Preserve"), protection);
    UMaterialEditingLibrary::ConnectMaterialProperty(composite, TEXT(""), MP_EmissiveColor);
    material->PostEditChange();
    auto const errors{UMaterialEditingLibrary::RecompileMaterial(material)};
    if (!errors.IsEmpty()) {
        UE_LOG(LogTemp, Error, TEXT("UI glow material compilation failed."));
        return false;
    }
    GShaderCompilingManager->FinishAllCompilation();
    FSavePackageArgs save_args;
    save_args.TopLevelFlags = RF_Public | RF_Standalone;
    for (UObject* const asset : {static_cast<UObject*>(texture), static_cast<UObject*>(material)}) {
        auto* const asset_package{asset->GetOutermost()};
        asset_package->MarkPackageDirty();
        auto const filename{FPackageName::LongPackageNameToFilename(
            asset_package->GetName(), FPackageName::GetAssetPackageExtension())};
        if (!UPackage::SavePackage(asset_package, asset, *filename, save_args)) {
            return false;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Generated %s"), SEntityOverlayWidget::glow_material_path);
    return true;
}
}
