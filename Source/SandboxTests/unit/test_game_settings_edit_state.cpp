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
};
