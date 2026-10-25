#include <SpaceGame/settings/ControlSettingsTypes.h>
#include <SpaceGame/settings/FlightModelSettingsCodec.h>
#include <SpaceGame/settings/GameSettingsBackend.h>
#include <SpaceGame/settings/GameSettingsEditState.h>
#include <SpaceGame/settings/GameSettingsSubsystem.h>
#include <SpaceGame/settings/SpaceGameUserSettings.h>

#include <CQTest.h>
#include <Engine/GameInstance.h>

TEST_CLASS(GameSettingsEditState, "Sandbox.UnitTests")
{
    using Setting = ioj::EGameSetting;
    using Category = ioj::EGameSettingCategory;
    using Device = ioj::EGameSettingDevice;
    using Quality = ioj::EGameQualityLevel;
    using Preset = ioj::EGameGraphicsPreset;
    using HardwareDevice = EHardwareDevicePrimaryType;

    TEST_METHOD(TracksPendingAppliedAndDefaultsByCategory)
    {
        ioj::FGameSettingsState applied;
        applied.vsync = false;
        applied.master_volume = 0.25f;
        applied.bees = 2;
        applied.mouse_turn_sensitivity = 0.25f;
        auto defaults{applied};
        defaults.vsync = true;
        defaults.master_volume = 1.0f;
        defaults.bees = 0;
        defaults.mouse_turn_sensitivity = 1.f;

        ioj::FGameSettingsEditState state;
        state.begin(applied, defaults);
        TestRunner->TestFalse(TEXT("A new edit session is clean"), state.is_dirty());
        TestRunner->TestFalse(TEXT("Non-default video values can be reset"),
                              state.is_at_defaults(Category::Video));

        state.set_setting(Setting::MasterVolume, ioj::FGameSettingValue{0.5f});
        TestRunner->TestTrue(TEXT("Editing a value makes its category dirty"),
                             state.is_dirty(Category::Audio));
        TestRunner->TestFalse(TEXT("Editing audio does not dirty video"),
                              state.is_dirty(Category::Video));

        state.reset_category(Category::Video);
        TestRunner->TestTrue(TEXT("Reset changes only the requested category"),
                             state.pending().vsync &&
                                 FMath::IsNearlyEqual(state.pending().master_volume, 0.5f));
        TestRunner->TestTrue(TEXT("Resetting to a different default creates a dirty value"),
                             state.is_dirty(Category::Video));
        TestRunner->TestTrue(TEXT("The reset category now matches defaults"),
                             state.is_at_defaults(Category::Video));

        state.commit_all();
        TestRunner->TestFalse(TEXT("Committing pending values clears dirty state"),
                              state.is_dirty());

        state.set_setting(Setting::Bees, ioj::FGameSettingValue{4});
        state.cancel();
        TestRunner->TestEqual(TEXT("Cancel restores the applied value"), state.pending().bees, 2);

        state.set_setting(Setting::MouseTurnSensitivity, ioj::FGameSettingValue{0.5f});
        TestRunner->TestTrue(TEXT("Input response is tracked as a Controls setting"),
                             state.is_dirty(Category::Controls));
        state.reset_category(Category::Controls);
        TestRunner->TestTrue(TEXT("Reset restores default input response"),
                             FMath::IsNearlyEqual(state.pending().mouse_turn_sensitivity, 1.f));
    }

    TEST_METHOD(AvailabilityUsesPendingState)
    {
        ioj::FGameSettingsBackend backend;
        ioj::FGameSettingsState state;
        state.aa_method = ioj::EGameAntiAliasingMethod::Off;
        TestRunner->TestFalse(
            TEXT("AA quality is disabled while anti-aliasing is off"),
            backend.is_available(ioj::EGameSettingAvailabilityProvider::AAQuality, state));

        state.aa_method = ioj::EGameAntiAliasingMethod::TAA;
        TestRunner->TestTrue(
            TEXT("AA quality is enabled from the pending anti-aliasing method"),
            backend.is_available(ioj::EGameSettingAvailabilityProvider::AAQuality, state));
    }

    TEST_METHOD(DisplayRevertKeepsAlreadyAppliedControls)
    {
        ioj::FGameSettingsState initial{};
        initial.resolution = FIntPoint{1920, 1080};
        initial.mouse_turn_sensitivity = 1.f;

        ioj::FGameSettingsEditState state;
        state.begin(initial, initial);
        auto expected{state.applied()};
        expected.mouse_turn_sensitivity = 1.5f;

        state.set_setting(Setting::Resolution, ioj::FGameSettingValue{FIntPoint{1280, 720}});
        state.set_setting(Setting::MouseTurnSensitivity, ioj::FGameSettingValue{1.5f});
        state.commit_setting(Setting::MouseTurnSensitivity);
        state.cancel();

        TestRunner->TestTrue(TEXT("Reverting display preserves only the applied controls change"),
                             state.pending() == expected);
        TestRunner->TestFalse(TEXT("Applied controls and reverted display leave clean state"),
                              state.is_dirty());
    }

    TEST_METHOD(AxisInversionUsesControlsTransaction)
    {
        using ioj::FGameSettingValue;

        for (auto const setting : {Setting::InvertMousePitch,
                                   Setting::InvertMouseYaw,
                                   Setting::InvertGamepadPitch,
                                   Setting::InvertGamepadYaw,
                                   Setting::InvertGamepadRoll}) {
            ioj::FGameSettingsState initial{};
            ioj::FGameSettingsEditState state;
            state.begin(initial, initial);
            state.set_setting(setting, FGameSettingValue{true});
            TestRunner->TestTrue(TEXT("Inversion previews as a controls edit"),
                                 state.value(setting) == FGameSettingValue{true} &&
                                     state.is_dirty(Category::Controls));
            state.cancel();
            TestRunner->TestTrue(TEXT("Cancel restores inversion"),
                                 state.value(setting) == FGameSettingValue{false});
            state.set_setting(setting, FGameSettingValue{true});
            state.commit_all();
            TestRunner->TestTrue(TEXT("Apply commits inversion"),
                                 state.value(setting) == FGameSettingValue{true} &&
                                     !state.is_dirty());
            state.reset_category(Category::Controls);
            TestRunner->TestTrue(TEXT("Reset previews default inversion"),
                                 state.value(setting) == FGameSettingValue{false});
            state.commit_all();
            TestRunner->TestTrue(TEXT("Reset plus Apply commits default inversion"),
                                 state.value(setting) == FGameSettingValue{false} &&
                                     !state.is_dirty());
        }
    }

    TEST_METHOD(GraphicsPresetUpdatesOnlyItsQualityGroups)
    {
        ioj::FGameSettingsState applied;
        applied.resolution_scale = 73.0f;
        applied.bloom = false;
        applied.motion_blur = true;

        ioj::FGameSettingsEditState state;
        state.begin(applied, applied);
        state.set_setting(Setting::OverallQuality, ioj::FGameSettingValue{Preset::High});

        auto expected{applied};
        expected.overall_quality = Preset::High;
        expected.view_distance_quality = Quality::High;
        expected.aa_quality = Quality::High;
        expected.shadow_quality = Quality::High;
        expected.global_illumination_quality = Quality::High;
        expected.reflections_quality = Quality::High;
        expected.post_processing_quality = Quality::High;
        expected.texture_quality = Quality::High;
        expected.effects_quality = Quality::High;
        expected.shading_quality = Quality::High;
        TestRunner->TestTrue(TEXT("The preset changes only its quality groups"),
                             state.pending() == expected);

        state.set_setting(Setting::ShadowQuality, ioj::FGameSettingValue{Quality::Medium});
        expected.shadow_quality = Quality::Medium;
        expected.overall_quality = Preset::Custom;
        TestRunner->TestTrue(TEXT("Changing one quality group produces a custom preset"),
                             state.pending() == expected);

        state.set_setting(Setting::OverallQuality, ioj::FGameSettingValue{Preset::Custom});
        TestRunner->TestTrue(TEXT("Selecting Custom preserves all settings"),
                             state.pending() == expected);
    }

    TEST_METHOD(ControlSettingsDeclareDeviceOwnership)
    {
        auto const device_for = [](Setting const setting) {
            return ioj::game_setting_descriptor(setting).device;
        };
        TestRunner->TestTrue(TEXT("Mouse sensitivity belongs to keyboard and mouse"),
                             device_for(Setting::MouseTurnSensitivity) == Device::KeyboardMouse);
        TestRunner->TestTrue(TEXT("Mouse inversion belongs to keyboard and mouse"),
                             device_for(Setting::InvertMousePitch) == Device::KeyboardMouse &&
                                 device_for(Setting::InvertMouseYaw) == Device::KeyboardMouse);
        TestRunner->TestTrue(TEXT("Controller sensitivity belongs to controller"),
                             device_for(Setting::GamepadTurnSensitivity) == Device::Controller);
        TestRunner->TestTrue(TEXT("Controller dead zones belong to controller"),
                             device_for(Setting::GamepadTurnDeadZone) == Device::Controller &&
                                 device_for(Setting::GamepadMoveDeadZone) == Device::Controller);
        TestRunner->TestTrue(TEXT("Controller inversion belongs to controller"),
                             device_for(Setting::InvertGamepadPitch) == Device::Controller &&
                                 device_for(Setting::InvertGamepadYaw) == Device::Controller &&
                                 device_for(Setting::InvertGamepadRoll) == Device::Controller &&
                                 device_for(Setting::InvertGamepadVerticalTranslation) ==
                                     Device::Controller);
    }

    TEST_METHOD(ControlBindingPresentationIdentityIsStable)
    {
        ioj::FControlBindingAddress first{
            .profile_id = TEXT("Profile.One"),
            .mapping_name = TEXT("Fire"),
            .hardware_device_id = TEXT("KeyboardMouse"),
            .slot = EPlayerMappableKeySlot::First,
        };
        auto second{first};
        second.profile_id = TEXT("Profile.Two");
        auto alternate{second};
        alternate.slot = EPlayerMappableKeySlot::Second;

        auto const first_identity{
            ioj::control_binding_identity(first, HardwareDevice::KeyboardAndMouse)};
        auto const second_identity{
            ioj::control_binding_identity(second, HardwareDevice::KeyboardAndMouse)};
        auto const alternate_identity{
            ioj::control_binding_identity(alternate, HardwareDevice::KeyboardAndMouse)};
        TestRunner->TestTrue(TEXT("Binding focus survives profile address replacement"),
                             first_identity == second_identity);
        TestRunner->TestFalse(TEXT("Multiple binding slots remain distinct"),
                              first_identity == alternate_identity);
    }

    TEST_METHOD(ControlBindingDeviceFilteringAndResetScopeAreExplicit)
    {
        ioj::FControlBindingView keyboard_binding{
            .device_type = HardwareDevice::KeyboardAndMouse,
        };
        ioj::FControlBindingView controller_binding{
            .device_type = HardwareDevice::Gamepad,
        };
        TestRunner->TestTrue(TEXT("Keyboard binding passes keyboard filtering"),
                             ioj::control_binding_matches_device(keyboard_binding,
                                                                 HardwareDevice::KeyboardAndMouse));
        TestRunner->TestFalse(TEXT("Controller binding does not pass keyboard filtering"),
                              ioj::control_binding_matches_device(
                                  controller_binding, HardwareDevice::KeyboardAndMouse));
        TestRunner->TestTrue(
            TEXT("Unspecified filtering includes both devices"),
            ioj::control_binding_matches_device(keyboard_binding, HardwareDevice::Unspecified) &&
                ioj::control_binding_matches_device(controller_binding,
                                                    HardwareDevice::Unspecified));
    }

    TEST_METHOD(FlightModelSlotsPersistIndependently)
    {
        auto* const user_settings{NewObject<ioj::USpaceGameUserSettings>()};
        auto loadout{ioj::sim::player::make_default_flight_model_loadout()};
        loadout.profiles[1].config.translation.forward.passive_drag = 321.f;
        loadout.profiles[1].customized = true;
        user_settings->set_flight_model_loadout(loadout);
        auto const restored{user_settings->flight_model_loadout()};
        TestRunner->TestTrue(TEXT("Flight-model loadout survives a settings round trip"),
                             restored == loadout);
    }

    TEST_METHOD(FlightModelCodecRejectsCorruptData)
    {
        auto profile{ioj::sim::player::make_flight_model_profile(
            ioj::sim::player::FlightModelPreset::Gunship)};
        auto const original{profile};
        TestRunner->TestFalse(TEXT("Invalid data is rejected"),
                              ioj::flight_model_settings_codec::decode(TEXT("broken"), profile));
        TestRunner->TestTrue(TEXT("Invalid data does not alter the profile"), profile == original);
        auto const encoded{ioj::flight_model_settings_codec::encode(profile)};
        profile.config.translation.up.passive_drag = 123.f;
        TestRunner->TestTrue(TEXT("Versioned profile decodes"),
                             ioj::flight_model_settings_codec::decode(encoded, profile));
        TestRunner->TestTrue(TEXT("Decoded profile matches original"), profile == original);
    }
};
