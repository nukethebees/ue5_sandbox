#include "SandboxEditor/material/MaterialEmitter.h"
#include "SandboxEditor/material/MaterialFrontend.h"
#include "SandboxEditor/material/MaterialSourceHash.h"

#include "CQTest.h"
#include "Engine/Texture.h"
#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"

namespace {

auto load_golden() -> material_synth::AnalysisResult {
    auto const path{FPaths::ConvertRelativePathToFull(
        FPaths::ProjectDir(),
        TEXT("Plugins/SandboxUI/Source/SandboxUI/Private/materials/"
             "UiGlowComposite.material.scm"))};
    FString source;
    if (!FFileHelper::LoadFileToString(source, *path)) {
        return {{}, {{TCHAR_TO_UTF8(*path), 1, 1, "unable to load golden source"}}};
    }
    auto const utf8_source{StringCast<UTF8CHAR>(*source)};
    auto const utf8_path{StringCast<UTF8CHAR>(*path)};
    return material_synth::analyze(
        reinterpret_cast<char const*>(utf8_path.Get()),
        std::string_view{reinterpret_cast<char const*>(utf8_source.Get()),
                         static_cast<std::size_t>(utf8_source.Length())},
        [](std::string_view const requested) -> std::optional<std::string> {
            auto package_path{FString{UTF8_TO_TCHAR(requested.data())}};
            auto leaf{package_path};
            int32 slash{};
            leaf.FindLastChar(TEXT('/'), slash);
            leaf.RightChopInline(slash + 1);
            auto const object_path{package_path + TEXT(".") + leaf};
            auto* const texture{LoadObject<UTexture>(nullptr, *object_path, nullptr, LOAD_NoWarn)};
            return texture != nullptr
                     ? std::optional<std::string>{TCHAR_TO_UTF8(*texture->GetPathName())}
                     : std::nullopt;
        });
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
        TestRunner->TestEqual(
            TEXT("SHA-256 matches the standard abc vector"),
            material_synth::sha256(
                MakeArrayView(reinterpret_cast<uint8 const*>(input), UE_ARRAY_COUNT(input) - 1)),
            FString{TEXT("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")});
    }

    TEST_METHOD(GeneratesCompilesSavesReloadsAndIsIdempotent)
    {
        auto const analysis{load_golden()};
        if (!TestRunner->TestTrue(TEXT("Golden source passes semantic analysis"),
                                  analysis.material.has_value())) {
            return;
        }

        auto const first{
            material_synth::emit(*analysis.material,
                                 TEXT("Plugins/SandboxUI/Source/SandboxUI/Private/materials/"
                                      "UiGlowComposite.material.scm"),
                                 TEXT("integration-test-sha256"))};
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
            FString{TEXT("Plugins/SandboxUI/Source/SandboxUI/Private/materials/"
                         "UiGlowComposite.material.scm")});
        TestRunner->TestEqual(
            TEXT("Source hash metadata is recorded"),
            FString{metadata.GetValue(first.material, material_synth::source_hash_key)},
            FString{TEXT("integration-test-sha256")});
        TestRunner->TestEqual(
            TEXT("Generator version metadata is recorded"),
            FString{metadata.GetValue(first.material, material_synth::version_key)},
            FString{material_synth::generator_version});

        auto const second{
            material_synth::emit(*analysis.material,
                                 TEXT("Plugins/SandboxUI/Source/SandboxUI/Private/materials/"
                                      "UiGlowComposite.material.scm"),
                                 TEXT("integration-test-sha256"))};
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
};
