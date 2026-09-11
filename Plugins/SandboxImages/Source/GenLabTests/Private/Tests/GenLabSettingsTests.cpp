#include "Editor/GenLabSettings.h"

#include <CQTest.h>

TEST_CLASS(GenLabPresetSettings, "SandboxImages.UnitTests")
{
    TEST_METHOD(CanonicalRequestsRoundTripThroughEditorSettings)
    {
        auto* const settings{NewObject<UGenLabSettings>()};
        TestRunner->TestNotNull(TEXT("Transient GenLab settings are created"), settings);
        if (settings == nullptr) {
            return;
        }

        for (auto const& expected : sandbox::image::default_generation_requests()) {
            settings->load_request(expected);
            auto const actual{settings->to_request()};
            auto const output_name{FString{UTF8_TO_TCHAR(expected.output_name.c_str())}};
            TestRunner->TestTrue(*FString::Printf(TEXT("%s output name"), *output_name),
                                 actual.output_name == expected.output_name);
            TestRunner->TestTrue(*FString::Printf(TEXT("%s parameters"), *output_name),
                                 sandbox::image::describe_request(actual) ==
                                     sandbox::image::describe_request(expected));
        }
    }

    TEST_METHOD(ChangingGeneratorPreservesSharedOutputShaping)
    {
        auto* const settings{NewObject<UGenLabSettings>()};
        settings->generator = EGenLabGenerator::CellularNoise;
        settings->invert = true;
        settings->contrast = 1.75f;
        settings->threshold_enabled = true;
        settings->threshold = 0.3f;
        settings->threshold_softness = 0.2f;
        settings->output = EGenLabOutput::NormalMap;
        settings->normal_strength = 5.0f;
        settings->normal_wrap = true;

        settings->load_generator_defaults();

        auto const request{settings->to_request()};
        TestRunner->TestEqual(TEXT("Generator defaults select cellular noise"),
                              request.generator,
                              sandbox::image::GeneratorType::CellularNoise);
        TestRunner->TestTrue(TEXT("Invert is preserved"), request.post_process.invert);
        TestRunner->TestEqual(TEXT("Contrast is preserved"), request.post_process.contrast, 1.75f);
        TestRunner->TestTrue(TEXT("Threshold is preserved"),
                             request.post_process.threshold_enabled);
        TestRunner->TestEqual(
            TEXT("Threshold value is preserved"), request.post_process.threshold, 0.3f);
        TestRunner->TestEqual(TEXT("Threshold softness is preserved"),
                              request.post_process.threshold_softness,
                              0.2f);
        TestRunner->TestEqual(TEXT("Output encoding is preserved"),
                              request.post_process.output,
                              sandbox::image::ImagePostProcessParameters::Output::NormalMap);
        TestRunner->TestEqual(
            TEXT("Normal strength is preserved"), request.post_process.normal_strength, 5.0f);
        TestRunner->TestTrue(TEXT("Normal wrapping is preserved"),
                             request.post_process.normal_wrap);
    }
};
