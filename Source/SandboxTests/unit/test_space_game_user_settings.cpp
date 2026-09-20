#include <SpaceGame/settings/SpaceGameUserSettings.h>

#include <CQTest.h>
#include <Engine/Engine.h>
#include <HAL/FileManager.h>
#include <HAL/IConsoleManager.h>
#include <Misc/ConfigCacheIni.h>
#include <Misc/FileHelper.h>
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
        TestRunner->TestEqual(TEXT("Gunship is the default flight control preset"),
                              source->player_ship_flight_control_preset(),
                              ml::ioj::EPlayerShipFlightControlPreset::Gunship);

        source->set_bloom_enabled(false);
        source->set_motion_blur_enabled(true);
        source->set_master_volume(0.35f);
        source->set_player_ship_flight_control_preset(
            ml::ioj::EPlayerShipFlightControlPreset::Skater);

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
        TestRunner->TestEqual(TEXT("Flight control preset survives a config round trip"),
                              loaded->player_ship_flight_control_preset(),
                              ml::ioj::EPlayerShipFlightControlPreset::Skater);
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

    TEST_METHOD(LegacyFlightControlPresetValuesMigrateExplicitly)
    {
        using Preset = ml::ioj::EPlayerShipFlightControlPreset;
        TArray<TPair<int32, Preset>> const cases{
            {0, Preset::Starfox},
            {1, Preset::Gunship},
            {2, Preset::Skater},
            {99, Preset::Gunship},
        };

        for (auto const& [legacy_value, expected] : cases) {
            auto const config_path{FPaths::CreateTempFilename(
                *FPaths::ProjectSavedDir(), TEXT("LegacyFlightModelSettings"), TEXT(".ini"))};
            ON_SCOPE_EXIT {
                GConfig->UnloadFile(config_path);
                IFileManager::Get().Delete(*config_path);
            };
            auto const config{FString::Printf(TEXT("[/Script/SpaceGame.SpaceGameUserSettings]\n")
                                                  TEXT("player_ship_flight_control_preset_=%d\n")
                                                      TEXT("flight_model_settings_version_=0\n"),
                                              legacy_value)};
            if (!TestRunner->TestTrue(TEXT("Legacy test config can be written"),
                                      FFileHelper::SaveStringToFile(config, *config_path))) {
                continue;
            }

            auto* const settings{NewObject<ml::ioj::USpaceGameUserSettings>()};
            settings->LoadConfig(ml::ioj::USpaceGameUserSettings::StaticClass(), *config_path);
            settings->ValidateSettings();

            TestRunner->TestEqual(TEXT("Legacy preset maps to the documented flight model"),
                                  settings->player_ship_flight_control_preset(),
                                  expected);
        }
    }
};
