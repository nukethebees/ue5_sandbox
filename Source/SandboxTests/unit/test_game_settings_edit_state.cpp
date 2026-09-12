#include <SpaceGame/settings/GameSettingsBackend.h>
#include <SpaceGame/settings/GameSettingsEditState.h>

#include <CQTest.h>

TEST_CLASS(GameSettingsEditState, "Sandbox.UnitTests")
{
    TEST_METHOD(TracksPendingAppliedAndDefaultsByCategory)
    {
        ml::ioj::FGameSettingsState applied;
        applied.vsync = false;
        applied.master_volume = 0.25f;
        applied.bees = 2;
        auto defaults{applied};
        defaults.vsync = true;
        defaults.master_volume = 1.0f;
        defaults.bees = 0;

        ml::ioj::FGameSettingsEditState state;
        state.begin(applied, defaults);
        TestRunner->TestFalse(TEXT("A new edit session is clean"), state.is_dirty());
        TestRunner->TestFalse(TEXT("Non-default video values can be reset"),
                              state.is_at_defaults(ml::ioj::EGameSettingCategory::Video));

        state.set_setting(ml::ioj::EGameSetting::MasterVolume, ml::ioj::FGameSettingValue{0.5f});
        TestRunner->TestTrue(TEXT("Editing a value makes its category dirty"),
                             state.is_dirty(ml::ioj::EGameSettingCategory::Audio));
        TestRunner->TestFalse(TEXT("Editing audio does not dirty video"),
                              state.is_dirty(ml::ioj::EGameSettingCategory::Video));

        state.reset_category(ml::ioj::EGameSettingCategory::Video);
        TestRunner->TestTrue(TEXT("Reset changes only the requested category"),
                             state.pending().vsync &&
                                 FMath::IsNearlyEqual(state.pending().master_volume, 0.5f));
        TestRunner->TestTrue(TEXT("Resetting to a different default creates a dirty value"),
                             state.is_dirty(ml::ioj::EGameSettingCategory::Video));
        TestRunner->TestTrue(TEXT("The reset category now matches defaults"),
                             state.is_at_defaults(ml::ioj::EGameSettingCategory::Video));

        state.commit_all();
        TestRunner->TestFalse(TEXT("Committing pending values clears dirty state"),
                              state.is_dirty());

        state.set_setting(ml::ioj::EGameSetting::Bees, ml::ioj::FGameSettingValue{4});
        state.cancel();
        TestRunner->TestEqual(TEXT("Cancel restores the applied value"), state.pending().bees, 2);
    }

    TEST_METHOD(AvailabilityUsesPendingState)
    {
        ml::ioj::FGameSettingsBackend backend;
        ml::ioj::FGameSettingsState state;
        state.aa_method = ml::ioj::EGameAntiAliasingMethod::Off;
        TestRunner->TestFalse(
            TEXT("AA quality is disabled while anti-aliasing is off"),
            backend.is_available(ml::ioj::EGameSettingAvailabilityProvider::AAQuality, state));

        state.aa_method = ml::ioj::EGameAntiAliasingMethod::TAA;
        TestRunner->TestTrue(
            TEXT("AA quality is enabled from the pending anti-aliasing method"),
            backend.is_available(ml::ioj::EGameSettingAvailabilityProvider::AAQuality, state));
    }

    TEST_METHOD(GraphicsPresetUpdatesOnlyItsQualityGroups)
    {
        ml::ioj::FGameSettingsState applied;
        applied.resolution_scale = 73.0f;
        applied.bloom = false;
        applied.motion_blur = true;

        ml::ioj::FGameSettingsEditState state;
        state.begin(applied, applied);
        state.set_setting(ml::ioj::EGameSetting::OverallQuality,
                          ml::ioj::FGameSettingValue{ml::ioj::EGameGraphicsPreset::High});

        auto const& preset{state.pending()};
        TestRunner->TestTrue(TEXT("The selected preset is reported"),
                             preset.overall_quality == ml::ioj::EGameGraphicsPreset::High);
        TestRunner->TestTrue(
            TEXT("The preset updates every player-facing scalability group"),
            preset.view_distance_quality == ml::ioj::EGameQualityLevel::High &&
                preset.aa_quality == ml::ioj::EGameQualityLevel::High &&
                preset.shadow_quality == ml::ioj::EGameQualityLevel::High &&
                preset.global_illumination_quality == ml::ioj::EGameQualityLevel::High &&
                preset.reflections_quality == ml::ioj::EGameQualityLevel::High &&
                preset.post_processing_quality == ml::ioj::EGameQualityLevel::High &&
                preset.texture_quality == ml::ioj::EGameQualityLevel::High &&
                preset.effects_quality == ml::ioj::EGameQualityLevel::High &&
                preset.shading_quality == ml::ioj::EGameQualityLevel::High);
        TestRunner->TestTrue(TEXT("Independent graphics controls are unchanged"),
                             FMath::IsNearlyEqual(preset.resolution_scale, 73.0f) &&
                                 !preset.bloom && preset.motion_blur);

        state.set_setting(ml::ioj::EGameSetting::ShadowQuality,
                          ml::ioj::FGameSettingValue{ml::ioj::EGameQualityLevel::Medium});
        TestRunner->TestTrue(TEXT("Changing one quality group produces a custom preset"),
                             state.pending().overall_quality ==
                                 ml::ioj::EGameGraphicsPreset::Custom);

        state.set_setting(ml::ioj::EGameSetting::OverallQuality,
                          ml::ioj::FGameSettingValue{ml::ioj::EGameGraphicsPreset::Custom});
        TestRunner->TestTrue(TEXT("Selecting Custom preserves the individual quality values"),
                             state.pending().shadow_quality == ml::ioj::EGameQualityLevel::Medium);
    }
};
