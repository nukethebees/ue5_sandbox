#include "SandboxEditor/material/MaterialEmitter.h"

#include <material_gen/CompiledMaterial.h>
#include <material_gen/SourceHash.h>

#include "CQTest.h"
#include "Engine/Texture.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace {

auto load_compiled(TCHAR const* const filename)
    -> std::expected<material_synth::CompiledMaterial, std::string> {
    auto const path{FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir(),
                                                      FString{TEXT("MaterialGen/")} + filename)};
    TArray<uint8> bytes;
    if (!FFileHelper::LoadFileToArray(bytes, *path)) {
        return std::unexpected{"unable to load compiled golden material"};
    }
    return material_synth::deserialize(
        std::span{bytes.GetData(), static_cast<std::size_t>(bytes.Num())});
}

auto topology(UMaterial const& material) -> TArray<FString> {
    TArray<FString> result;
    for (auto const expression : material.GetExpressions()) {
        result.Add(expression->GetClass()->GetPathName());
    }
    return result;
}

}

TEST_CLASS(MaterialSynth, "SandboxEditor.MaterialSynth")
{
    TEST_METHOD(ComputesSha256)
    {
        constexpr ANSICHAR input[]{"abc"};
        auto const hash{material_synth::sha256(
            std::span{reinterpret_cast<std::uint8_t const*>(input), UE_ARRAY_COUNT(input) - 1})};
        TestRunner->TestEqual(
            TEXT("SHA-256 matches the standard abc vector"),
            FString{UTF8_TO_TCHAR(hash.c_str())},
            FString{TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")});
    }

    TEST_METHOD(GeneratesCompilesSavesReloadsAndIsIdempotent)
    {
        auto const compiled{load_compiled(TEXT("UiGlowComposite.smat"))};
        if (!TestRunner->TestTrue(TEXT("Compiled golden material loads"), compiled.has_value())) {
            return;
        }

        auto const source_path{FString{UTF8_TO_TCHAR(compiled->source_path.c_str())}};
        auto const source_hash{FString{UTF8_TO_TCHAR(compiled->source_hash.c_str())}};
        auto const first{material_synth::emit(compiled->material, source_path, source_hash)};
        if (!TestRunner->TestTrue(TEXT("First generation succeeds"),
                                  first.material != nullptr && first.errors.IsEmpty())) {
            return;
        }
        auto const first_topology{topology(*first.material)};

        TestRunner->TestEqual(TEXT("Material domain is UI"), first.material->MaterialDomain, MD_UI);
        TestRunner->TestEqual(
            TEXT("Blend mode is additive"), first.material->BlendMode, BLEND_Additive);
        TestRunner->TestEqual(TEXT("Six expressions are emitted"), first_topology.Num(), 6);

        int32 parameter_count{};
        UMaterialExpressionCustom const* custom{};
        for (auto const expression : first.material->GetExpressions()) {
            if (expression->IsA<UMaterialExpressionScalarParameter>() ||
                expression->IsA<UMaterialExpressionVectorParameter>() ||
                expression->IsA<UMaterialExpressionTextureObjectParameter>()) {
                ++parameter_count;
            }
            if (auto const* candidate{Cast<UMaterialExpressionCustom>(expression)}) {
                custom = candidate;
            }
        }
        TestRunner->TestEqual(TEXT("Four parameters are emitted"), parameter_count, 4);
        if (TestRunner->TestNotNull(TEXT("Custom expression is emitted"), custom)) {
            TestRunner->TestEqual(
                TEXT("Custom expression has five inputs"), custom->Inputs.Num(), 5);
            if (custom->Inputs.Num() == 5) {
                TestRunner->TestEqual(TEXT("Custom input order starts with Energy"),
                                      custom->Inputs[0].InputName,
                                      FName{TEXT("Energy")});
                TestRunner->TestEqual(TEXT("Custom input order ends with Preserve"),
                                      custom->Inputs[4].InputName,
                                      FName{TEXT("Preserve")});
            }
        }
        auto const* emissive{first.material->GetExpressionInputForProperty(MP_EmissiveColor)};
        TestRunner->TestTrue(TEXT("Emissive is connected to Custom"),
                             emissive != nullptr && emissive->Expression == custom);
        auto& metadata{first.material->GetOutermost()->GetMetaData()};
        TestRunner->TestEqual(
            TEXT("Ownership metadata is recorded"),
            FString{metadata.GetValue(first.material, material_synth::ownership_key)},
            FString{material_synth::generator_version});
        TestRunner->TestEqual(
            TEXT("Source metadata is recorded"),
            FString{metadata.GetValue(first.material, material_synth::source_key)},
            source_path);
        TestRunner->TestEqual(
            TEXT("Source hash metadata is recorded"),
            FString{metadata.GetValue(first.material, material_synth::source_hash_key)},
            source_hash);
        TestRunner->TestEqual(
            TEXT("Generator version metadata is recorded"),
            FString{metadata.GetValue(first.material, material_synth::version_key)},
            FString{material_synth::generator_version});

        auto const second{material_synth::emit(compiled->material, source_path, source_hash)};
        if (TestRunner->TestTrue(TEXT("Second generation succeeds"),
                                 second.material != nullptr && second.errors.IsEmpty())) {
            TestRunner->TestTrue(TEXT("Regeneration preserves structural topology"),
                                 topology(*second.material) == first_topology);
        }

        auto* const loaded_package{LoadPackage(
            nullptr, TEXT("/SandboxUI/Generated/Materials/M_UiGlowComposite"), LOAD_None)};
        auto* const reloaded{loaded_package != nullptr
                                 ? FindObject<UMaterial>(loaded_package, TEXT("M_UiGlowComposite"))
                                 : nullptr};
        TestRunner->TestNotNull(TEXT("Saved material reloads from its package"), reloaded);
    }

    TEST_METHOD(GeneratesWorldSurfaceMaterialWithPerInstanceRangeData)
    {
        auto const compiled{load_compiled(TEXT("SoftTargetWorld.smat"))};
        if (!TestRunner->TestTrue(TEXT("Compiled world material loads"), compiled.has_value())) {
            return;
        }

        auto const source_path{FString{UTF8_TO_TCHAR(compiled->source_path.c_str())}};
        auto const source_hash{FString{UTF8_TO_TCHAR(compiled->source_hash.c_str())}};
        auto const emitted{material_synth::emit(compiled->material, source_path, source_hash)};
        if (!TestRunner->TestTrue(TEXT("World material generation succeeds"),
                                  emitted.material != nullptr && emitted.errors.IsEmpty())) {
            return;
        }

        auto& material{*emitted.material};
        TestRunner->TestEqual(
            TEXT("Material domain is surface"), material.MaterialDomain, MD_Surface);
        TestRunner->TestEqual(
            TEXT("Blend mode is translucent"), material.BlendMode, BLEND_Translucent);
        TestRunner->TestTrue(TEXT("Material is unlit"),
                             material.GetShadingModels().HasShadingModel(MSM_Unlit));
        TestRunner->TestTrue(TEXT("Material is two-sided"), material.TwoSided);
        TestRunner->TestTrue(TEXT("Material disables depth testing"), material.bDisableDepthTest);
        TestRunner->TestTrue(TEXT("Material supports instanced static meshes"),
                             material.GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));

        TArray<int32> custom_data_indices;
        for (auto const expression : material.GetExpressions()) {
            if (auto const* custom_data{
                    Cast<UMaterialExpressionPerInstanceCustomData>(expression)}) {
                custom_data_indices.Add(custom_data->DataIndex);
            }
        }
        custom_data_indices.Sort();
        TestRunner->TestEqual(
            TEXT("Six per-instance values are emitted"), custom_data_indices.Num(), 6);
        for (int32 index{}; index < custom_data_indices.Num(); ++index) {
            TestRunner->TestEqual(TEXT("Per-instance data indices remain contiguous"),
                                  custom_data_indices[index],
                                  index);
        }

        auto const* emissive{material.GetExpressionInputForProperty(MP_EmissiveColor)};
        auto const* opacity{material.GetExpressionInputForProperty(MP_Opacity)};
        TestRunner->TestTrue(TEXT("Emissive output is connected"),
                             emissive != nullptr && emissive->Expression != nullptr);
        TestRunner->TestTrue(TEXT("Opacity output is connected"),
                             opacity != nullptr && opacity->Expression != nullptr);
    }
};
