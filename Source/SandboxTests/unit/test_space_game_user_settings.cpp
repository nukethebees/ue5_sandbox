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
    TEST_METHOD(DefaultsAndConfigRoundTripPreserveRenderingEffects)
    {
        auto* const source{NewObject<ml::ioj::USpaceGameUserSettings>()};
        source->SetToDefaults();
        TestRunner->TestTrue(TEXT("Bloom defaults to enabled"), source->bloom_enabled());
        TestRunner->TestFalse(TEXT("Motion blur defaults to disabled"),
                              source->motion_blur_enabled());

        source->set_bloom_enabled(false);
        source->set_motion_blur_enabled(true);
        source->set_master_volume(0.35f);

        auto const config_path{FPaths::CreateTempFilename(
            *FPaths::ProjectSavedDir(), TEXT("SpaceGameUserSettings"), TEXT(".ini"))};
        ON_SCOPE_EXIT {
            GConfig->UnloadFile(config_path);
            IFileManager::Get().Delete(*config_path);
        };

        source->SaveConfig(CPF_Config, *config_path);
        GConfig->UnloadFile(config_path);

        auto* const loaded{NewObject<ml::ioj::USpaceGameUserSettings>()};
        loaded->LoadConfig(ml::ioj::USpaceGameUserSettings::StaticClass(), *config_path);

        TestRunner->TestFalse(TEXT("Bloom survives a config round trip"), loaded->bloom_enabled());
        TestRunner->TestTrue(TEXT("Motion blur survives a config round trip"),
                             loaded->motion_blur_enabled());
        TestRunner->TestTrue(TEXT("Existing custom settings still round trip"),
                             FMath::IsNearlyEqual(loaded->master_volume(), 0.35f));
    }

    TEST_METHOD(RenderingEffectOverridesTakePrecedenceOverScalability)
    {
        auto* const live_settings{GEngine != nullptr ? Cast<ml::ioj::USpaceGameUserSettings>(
                                                           GEngine->GetGameUserSettings())
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

        auto* const settings{NewObject<ml::ioj::USpaceGameUserSettings>()};
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
