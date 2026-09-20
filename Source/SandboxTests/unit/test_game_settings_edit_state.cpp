#include <SpaceGame/settings/ControlSettingsTypes.h>
#include <SpaceGame/settings/GameSettingsBackend.h>
#include <SpaceGame/settings/GameSettingsEditState.h>
#include <SpaceGame/settings/GameSettingsSubsystem.h>

#include <CQTest.h>

TEST_CLASS(GameSettingsEditState, "Sandbox.UnitTests")
{
    TEST_METHOD(TracksPendingAppliedAndDefaultsByCategory)
    {
        ml::ioj::FGameSettingsState applied;
        applied.vsync = false;
        applied.master_volume = 0.25f;
        applied.bees = 2;
        applied.player_ship_flight_control_preset =
            ml::ioj::EPlayerShipFlightControlPreset::Gunship;
        auto defaults{applied};
        defaults.vsync = true;
        defaults.master_volume = 1.0f;
        defaults.bees = 0;
        defaults.player_ship_flight_control_preset =
            ml::ioj::EPlayerShipFlightControlPreset::Starfox;

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

        state.set_setting(
            ml::ioj::EGameSetting::PlayerShipFlightControlPreset,
            ml::ioj::FGameSettingValue{ml::ioj::EPlayerShipFlightControlPreset::Skater});
        TestRunner->TestTrue(TEXT("Flight controls are tracked as a Controls setting"),
                             state.is_dirty(ml::ioj::EGameSettingCategory::Controls));
        state.reset_category(ml::ioj::EGameSettingCategory::Controls);
        TestRunner->TestEqual(TEXT("Reset restores the default flight control preset"),
                              state.pending().player_ship_flight_control_preset,
                              ml::ioj::EPlayerShipFlightControlPreset::Starfox);
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

    TEST_METHOD(ControlSettingsDeclareDeviceOwnership)
    {
        auto const device_for = [](ml::ioj::EGameSetting const setting) {
            return ml::ioj::game_setting_descriptor(setting).device;
        };
        TestRunner->TestTrue(TEXT("Flight controls are shared"),
                             device_for(ml::ioj::EGameSetting::PlayerShipFlightControlPreset) ==
                                 ml::ioj::EGameSettingDevice::Shared);
        TestRunner->TestTrue(TEXT("Mouse sensitivity belongs to keyboard and mouse"),
                             device_for(ml::ioj::EGameSetting::MouseTurnSensitivity) ==
                                 ml::ioj::EGameSettingDevice::KeyboardMouse);
        TestRunner->TestTrue(TEXT("Mouse inversion belongs to keyboard and mouse"),
                             device_for(ml::ioj::EGameSetting::InvertMousePitch) ==
                                 ml::ioj::EGameSettingDevice::KeyboardMouse);
        TestRunner->TestTrue(TEXT("Controller sensitivity belongs to controller"),
                             device_for(ml::ioj::EGameSetting::GamepadTurnSensitivity) ==
                                 ml::ioj::EGameSettingDevice::Controller);
        TestRunner->TestTrue(TEXT("Controller dead zones belong to controller"),
                             device_for(ml::ioj::EGameSetting::GamepadTurnDeadZone) ==
                                     ml::ioj::EGameSettingDevice::Controller &&
                                 device_for(ml::ioj::EGameSetting::GamepadMoveDeadZone) ==
                                     ml::ioj::EGameSettingDevice::Controller);
        TestRunner->TestTrue(TEXT("Controller inversion belongs to controller"),
                             device_for(ml::ioj::EGameSetting::InvertGamepadPitch) ==
                                 ml::ioj::EGameSettingDevice::Controller);
    }

    TEST_METHOD(ControlBindingPresentationIdentityIsStable)
    {
        ml::ioj::FControlBindingAddress first{
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
            ml::ioj::control_binding_identity(first, EHardwareDevicePrimaryType::KeyboardAndMouse)};
        auto const second_identity{ml::ioj::control_binding_identity(
            second, EHardwareDevicePrimaryType::KeyboardAndMouse)};
        auto const alternate_identity{ml::ioj::control_binding_identity(
            alternate, EHardwareDevicePrimaryType::KeyboardAndMouse)};
        TestRunner->TestTrue(TEXT("Binding focus survives profile address replacement"),
                             first_identity == second_identity);
        TestRunner->TestFalse(TEXT("Multiple binding slots remain distinct"),
                              first_identity == alternate_identity);
    }

    TEST_METHOD(ControlBindingDeviceFilteringAndResetScopeAreExplicit)
    {
        ml::ioj::FControlBindingView keyboard_binding{
            .device_type = EHardwareDevicePrimaryType::KeyboardAndMouse,
        };
        ml::ioj::FControlBindingView controller_binding{
            .device_type = EHardwareDevicePrimaryType::Gamepad,
        };
        TestRunner->TestTrue(TEXT("Keyboard binding passes keyboard filtering"),
                             ml::ioj::control_binding_matches_device(
                                 keyboard_binding, EHardwareDevicePrimaryType::KeyboardAndMouse));
        TestRunner->TestFalse(
            TEXT("Controller binding does not pass keyboard filtering"),
            ml::ioj::control_binding_matches_device(controller_binding,
                                                    EHardwareDevicePrimaryType::KeyboardAndMouse));
        TestRunner->TestTrue(TEXT("Unspecified filtering includes both devices"),
                             ml::ioj::control_binding_matches_device(
                                 keyboard_binding, EHardwareDevicePrimaryType::Unspecified) &&
                                 ml::ioj::control_binding_matches_device(
                                     controller_binding, EHardwareDevicePrimaryType::Unspecified));
        TestRunner->TestTrue(TEXT("Built-in profiles reset all controls"),
                             ml::ioj::control_reset_scope(false) ==
                                 ml::ioj::EControlResetScope::AllControls);
        TestRunner->TestTrue(TEXT("Custom profiles reset response settings only"),
                             ml::ioj::control_reset_scope(true) ==
                                 ml::ioj::EControlResetScope::SettingsOnly);
    }

    TEST_METHOD(RuntimeFlightModelEditsAreValidatedAndMarkedCustom)
    {
        auto* const settings{NewObject<ml::ioj::UGameSettingsSubsystem>()};
        int32 change_count{};
        settings->flight_model_config_changed.AddLambda([&change_count] { ++change_count; });

        auto profile{::ioj::sim::player::make_flight_model_profile(
            ::ioj::sim::player::FlightModelPreset::Fighter)};
        profile.config.translation.forward.passive_drag = 321.f;
        TestRunner->TestTrue(TEXT("Valid runtime flight-model edits are accepted"),
                             settings->set_flight_model_profile(profile));
        TestRunner->TestTrue(TEXT("A runtime edit marks the profile custom"),
                             settings->flight_model_profile().customized);
        TestRunner->TestTrue(
            TEXT("The edited underlying value remains inspectable"),
            FMath::IsNearlyEqual(
                settings->flight_model_profile().config.translation.forward.passive_drag, 321.f));
        TestRunner->TestEqual(
            TEXT("Accepted edits emit the targeted change signal"), change_count, 1);

        auto invalid{settings->flight_model_profile()};
        invalid.config.translation.forward.passive_drag = -1.f;
        TestRunner->TestFalse(TEXT("Invalid runtime flight-model edits are rejected"),
                              settings->set_flight_model_profile(invalid));
        TestRunner->TestTrue(
            TEXT("Rejected edits preserve the previous runtime profile"),
            FMath::IsNearlyEqual(
                settings->flight_model_profile().config.translation.forward.passive_drag, 321.f));
        TestRunner->TestEqual(TEXT("Rejected edits do not emit a change signal"), change_count, 1);

        auto observed{::ioj::sim::player::make_flight_model_profile(
            ::ioj::sim::player::FlightModelPreset::Skater)};
        TestRunner->TestTrue(TEXT("A valid native selection can update the editor view"),
                             settings->observe_flight_model_profile(observed));
        TestRunner->TestTrue(TEXT("Observing a selection does not mark it custom"),
                             settings->flight_model_profile().base_preset ==
                                     ::ioj::sim::player::FlightModelPreset::Skater &&
                                 !settings->flight_model_profile().customized);
        TestRunner->TestEqual(
            TEXT("Observing a native selection does not reapply configuration"), change_count, 1);
    }
};
