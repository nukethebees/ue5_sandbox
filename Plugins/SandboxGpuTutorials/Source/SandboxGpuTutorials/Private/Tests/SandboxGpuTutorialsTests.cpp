#include "Lessons/Lesson03/Lesson03State.h"

#include "MaterialDomain.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"

#include <CQTest.h>

namespace {
struct FMaterialContract {
    TCHAR const* object_path;
    TCHAR const* include_path;
    TCHAR const* function_name;
};

auto find_custom_expression(UMaterial const& material) -> UMaterialExpressionCustom const* {
    for (auto const& expression : material.GetExpressions()) {
        if (auto const* custom{Cast<UMaterialExpressionCustom>(expression)}) {
            return custom;
        }
    }
    return nullptr;
}

auto contains_parameter(TConstArrayView<FMaterialParameterInfo> const parameters, FName const name)
    -> bool {
    return parameters.ContainsByPredicate(
        [name](FMaterialParameterInfo const& parameter) { return parameter.Name == name; });
}
}

TEST_CLASS(SandboxGpuTutorialMaterials, "SandboxGpuTutorials.UnitTests")
{
    TEST_METHOD(AssetsUseUiMaterialsAndExternalLessonIncludes)
    {
        FMaterialContract const contracts[]{
            {TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson01/"
                  "M_Lesson01_Gradient.M_Lesson01_Gradient"),
             TEXT("/Plugin/SandboxGpuTutorials/Lesson01/Lesson01.ush"),
             TEXT("sandbox_gpu_lesson01_gradient")},
            {TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson02/"
                  "M_Lesson02_Coordinates.M_Lesson02_Coordinates"),
             TEXT("/Plugin/SandboxGpuTutorials/Lesson02/Lesson02.ush"),
             TEXT("sandbox_gpu_lesson02_coordinates")},
            {TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson02/"
                  "M_Lesson02_AspectRatio.M_Lesson02_AspectRatio"),
             TEXT("/Plugin/SandboxGpuTutorials/Lesson02/Lesson02.ush"),
             TEXT("sandbox_gpu_lesson02_aspect_ratio")},
            {TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson02/"
                  "M_Lesson02_Circles.M_Lesson02_Circles"),
             TEXT("/Plugin/SandboxGpuTutorials/Lesson02/Lesson02.ush"),
             TEXT("sandbox_gpu_lesson02_circles")},
            {TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson02/"
                  "M_Lesson02_Boxes.M_Lesson02_Boxes"),
             TEXT("/Plugin/SandboxGpuTutorials/Lesson02/Lesson02.ush"),
             TEXT("sandbox_gpu_lesson02_boxes")},
            {TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson02/"
                  "M_Lesson02_Grid.M_Lesson02_Grid"),
             TEXT("/Plugin/SandboxGpuTutorials/Lesson02/Lesson02.ush"),
             TEXT("sandbox_gpu_lesson02_grid")},
            {TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson03/"
                  "M_Lesson03_AnimatedRings.M_Lesson03_AnimatedRings"),
             TEXT("/Plugin/SandboxGpuTutorials/Lesson03/Lesson03.ush"),
             TEXT("sandbox_gpu_lesson03_animated_rings")},
        };

        for (auto const& contract : contracts) {
            auto* const material{LoadObject<UMaterial>(nullptr, contract.object_path)};
            if (!TestRunner->TestNotNull(contract.object_path, material)) {
                continue;
            }

            TestRunner->TestEqual(
                TEXT("Tutorial material uses the UI domain"), material->MaterialDomain, MD_UI);
            auto const* custom{find_custom_expression(*material)};
            if (TestRunner->TestNotNull(TEXT("Material has a Custom expression"), custom)) {
                TestRunner->TestTrue(TEXT("Custom expression names its external lesson include"),
                                     custom->IncludeFilePaths.Contains(contract.include_path));
                TestRunner->TestTrue(TEXT("Custom expression calls its lesson function"),
                                     custom->Code.Contains(contract.function_name));
            }
        }
    }

    TEST_METHOD(Lesson03MaterialExposesItsCpuGpuParameterContract)
    {
        auto* const material{
            LoadObject<UMaterial>(nullptr,
                                  TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson03/"
                                       "M_Lesson03_AnimatedRings.M_Lesson03_AnimatedRings"))};
        if (!TestRunner->TestNotNull(TEXT("Lesson 03 material loads"), material)) {
            return;
        }

        TArray<FMaterialParameterInfo> scalar_parameters;
        TArray<FMaterialParameterInfo> vector_parameters;
        TArray<FGuid> parameter_ids;
        material->GetAllScalarParameterInfo(scalar_parameters, parameter_ids);
        parameter_ids.Reset();
        material->GetAllVectorParameterInfo(vector_parameters, parameter_ids);

        TestRunner->TestTrue(TEXT("TimeSeconds exists"),
                             contains_parameter(scalar_parameters, TEXT("TimeSeconds")));
        TestRunner->TestTrue(TEXT("RingThickness exists"),
                             contains_parameter(scalar_parameters, TEXT("RingThickness")));
        TestRunner->TestTrue(TEXT("AnimationSpeed exists"),
                             contains_parameter(scalar_parameters, TEXT("AnimationSpeed")));
        TestRunner->TestTrue(TEXT("PulseAmount exists"),
                             contains_parameter(scalar_parameters, TEXT("PulseAmount")));
        TestRunner->TestTrue(TEXT("PrimaryColor exists"),
                             contains_parameter(vector_parameters, TEXT("PrimaryColor")));

        float ring_thickness{};
        float animation_speed{};
        float pulse_amount{};
        FLinearColor primary_color{};
        TestRunner->TestTrue(
            TEXT("RingThickness has a default"),
            material->GetScalarParameterDefaultValue(
                FHashedMaterialParameterInfo{TEXT("RingThickness")}, ring_thickness));
        TestRunner->TestTrue(
            TEXT("AnimationSpeed has a default"),
            material->GetScalarParameterDefaultValue(
                FHashedMaterialParameterInfo{TEXT("AnimationSpeed")}, animation_speed));
        TestRunner->TestTrue(TEXT("PulseAmount has a default"),
                             material->GetScalarParameterDefaultValue(
                                 FHashedMaterialParameterInfo{TEXT("PulseAmount")}, pulse_amount));
        TestRunner->TestTrue(
            TEXT("PrimaryColor has a default"),
            material->GetVectorParameterDefaultValue(
                FHashedMaterialParameterInfo{TEXT("PrimaryColor")}, primary_color));
        TestRunner->TestEqual(TEXT("RingThickness default matches CPU state"),
                              ring_thickness,
                              FLesson03State{}.ring_thickness);
        TestRunner->TestEqual(TEXT("AnimationSpeed default matches CPU state"),
                              animation_speed,
                              FLesson03State{}.animation_speed);
        TestRunner->TestEqual(TEXT("PulseAmount default matches CPU state"),
                              pulse_amount,
                              FLesson03State{}.pulse_amount);
        TestRunner->TestEqual(TEXT("PrimaryColor default matches CPU state"),
                              primary_color,
                              FLesson03State{}.primary_color);
    }

    TEST_METHOD(Lesson02AspectRatioMatchesItsPreview)
    {
        auto* const material{
            LoadObject<UMaterial>(nullptr,
                                  TEXT("/SandboxGpuTutorials/Showcases/Materials/Lesson02/"
                                       "M_Lesson02_AspectRatio.M_Lesson02_AspectRatio"))};
        if (!TestRunner->TestNotNull(TEXT("Lesson 02 aspect material loads"), material)) {
            return;
        }

        float aspect_ratio{};
        TestRunner->TestTrue(TEXT("AspectRatio has a default"),
                             material->GetScalarParameterDefaultValue(
                                 FHashedMaterialParameterInfo{TEXT("AspectRatio")}, aspect_ratio));
        TestRunner->TestEqual(
            TEXT("AspectRatio matches the 520 by 260 preview"), aspect_ratio, 2.0f);
    }
};

TEST_CLASS(SandboxGpuTutorialAnimationState, "SandboxGpuTutorials.UnitTests")
{
    TEST_METHOD(AdvancesOnlyWhileAnimationIsEnabled)
    {
        FLesson03State state;
        state.advance(0.25f);
        TestRunner->TestEqual(
            TEXT("Enabled animation advances elapsed time"), state.elapsed_time, 0.25f);

        state.animation_enabled = false;
        state.advance(0.75f);
        TestRunner->TestEqual(
            TEXT("Paused animation preserves elapsed time"), state.elapsed_time, 0.25f);
    }

    TEST_METHOD(DefaultsMatchTheMaterialContract)
    {
        FLesson03State const state;
        TestRunner->TestEqual(TEXT("Default ring thickness"), state.ring_thickness, 0.018f);
        TestRunner->TestEqual(TEXT("Default animation speed"), state.animation_speed, 1.0f);
        TestRunner->TestEqual(TEXT("Default pulse amount"), state.pulse_amount, 0.08f);
        TestRunner->TestTrue(TEXT("Animation starts enabled"), state.animation_enabled);
    }

    TEST_METHOD(SliderMappingsCoverTheDocumentedParameterRanges)
    {
        FLesson03State state;

        state.set_ring_thickness_from_slider(0.0f);
        TestRunner->TestEqual(TEXT("Ring slider minimum"),
                              state.ring_thickness,
                              FLesson03State::minimum_ring_thickness);
        state.set_ring_thickness_from_slider(1.0f);
        TestRunner->TestTrue(
            TEXT("Ring slider maximum"),
            FMath::IsNearlyEqual(state.ring_thickness, FLesson03State::maximum_ring_thickness));
        TestRunner->TestTrue(TEXT("Ring slider inverse"),
                             FMath::IsNearlyEqual(state.ring_thickness_slider_value(), 1.0f));

        state.set_animation_speed_from_slider(1.0f);
        TestRunner->TestEqual(TEXT("Speed slider maximum"),
                              state.animation_speed,
                              FLesson03State::maximum_animation_speed);
        TestRunner->TestEqual(
            TEXT("Speed slider inverse"), state.animation_speed_slider_value(), 1.0f);

        state.set_pulse_amount_from_slider(1.0f);
        TestRunner->TestEqual(
            TEXT("Pulse slider maximum"), state.pulse_amount, FLesson03State::maximum_pulse_amount);
        TestRunner->TestEqual(
            TEXT("Pulse slider inverse"), state.pulse_amount_slider_value(), 1.0f);
    }
};
