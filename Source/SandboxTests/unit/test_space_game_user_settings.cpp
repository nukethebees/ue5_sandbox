#include <SpaceGame/settings/SpaceGameUserSettings.h>

#include <CQTest.h>
#include <Engine/Engine.h>
#include <HAL/FileManager.h>
#include <HAL/IConsoleManager.h>
#include <Misc/ConfigCacheIni.h>
#include <Misc/Paths.h>
#include <Misc/ScopeExit.h>

TEST_CLASS(SpaceGameUserSettings, "Sandbox.UnitTests")
{
    TEST_METHOD(RenderingEffectOverridesTakePrecedenceOverScalability)
    {
        auto* const live_settings{
            GEngine != nullptr ? Cast<ioj::USpaceGameUserSettings>(GEngine->GetGameUserSettings())
                               : nullptr};
        if (!TestRunner->TestNotNull(TEXT("SpaceGame user settings are configured"),
                                     live_settings)) {
            return;
        }
        ON_SCOPE_EXIT {
            live_settings->ApplyNonResolutionSettings();
        };

        auto* const bloom_quality{
            IConsoleManager::Get().FindConsoleVariable(TEXT("r.BloomQuality"))};
        auto* const motion_blur_quality{
            IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality"))};
        if (!TestRunner->TestNotNull(TEXT("Bloom quality cvar exists"), bloom_quality) ||
            !TestRunner->TestNotNull(TEXT("Motion blur quality cvar exists"),
                                     motion_blur_quality)) {
            return;
        }

        auto* const settings{NewObject<ioj::USpaceGameUserSettings>()};
        settings->SetToDefaults();
        settings->SetPostProcessingQuality(3);
        settings->set_bloom_enabled(false);
        settings->set_motion_blur_enabled(false);
        settings->ApplyNonResolutionSettings();

        TestRunner->TestEqual(TEXT("Bloom Off maps to quality zero"), bloom_quality->GetInt(), 0);
        TestRunner->TestEqual(
            TEXT("Motion Blur Off maps to quality zero"), motion_blur_quality->GetInt(), 0);

        settings->SetPostProcessingQuality(0);
        settings->ApplyNonResolutionSettings();
        TestRunner->TestEqual(
            TEXT("Low post processing cannot re-enable Bloom"), bloom_quality->GetInt(), 0);
        TestRunner->TestEqual(TEXT("Low post processing cannot re-enable Motion Blur"),
                              motion_blur_quality->GetInt(),
                              0);

        settings->set_bloom_enabled(true);
        settings->set_motion_blur_enabled(true);
        settings->ApplyNonResolutionSettings();
        TestRunner->TestTrue(TEXT("Bloom On has a non-zero quality"), bloom_quality->GetInt() > 0);
        TestRunner->TestTrue(TEXT("Motion Blur On has a non-zero quality fallback"),
                             motion_blur_quality->GetInt() > 0);
    }
};
