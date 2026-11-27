#include <ioj/settings/control_binding_address.h>
#include <ioj/settings/control_binding_snapshot.h>
#include <ioj/settings/control_bindings.h>
#include <ioj/settings/game_anti_aliasing_method.h>
#include <ioj/settings/game_graphics_preset.h>
#include <ioj/settings/game_quality_level.h>
#include <ioj/settings/game_setting.h>
#include <ioj/settings/game_setting_availability_provider.h>
#include <ioj/settings/game_setting_category.h>
#include <ioj/settings/game_setting_device.h>
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings.h>
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/screen_resolution.h>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <limits>
#include <utility>
#include <vector>

namespace ioj::tests {
TEST(GameSettings, TracksCategoryResetCommitAndCancellation) {
    GameSettingsState applied;
    applied.audio.master_volume = 0.25f;
    auto defaults{applied};
    defaults.video.vsync = true;
    defaults.audio.master_volume = 1.f;
    GameSettingsEditState state;
    state.begin(applied, defaults);
    EXPECT_FALSE(state.is_dirty());
    ASSERT_TRUE(state.set_setting(GameSetting::MasterVolume, 0.5f));
    EXPECT_TRUE(state.is_dirty(GameSettingCategory::Audio));
    EXPECT_FALSE(state.is_dirty(GameSettingCategory::Video));
    state.reset_category(GameSettingCategory::Video);
    EXPECT_TRUE(state.is_at_defaults(GameSettingCategory::Video));
    EXPECT_EQ(state.pending().audio.master_volume, 0.5f);
    EXPECT_TRUE(state.is_dirty(GameSettingCategory::Video));
    state.commit_all();
    EXPECT_FALSE(state.is_dirty());
    ASSERT_TRUE(state.set_setting(GameSetting::MotionBlur, true));
    state.cancel();
    EXPECT_FALSE(state.pending().video.motion_blur);
    ASSERT_TRUE(state.set_setting(GameSetting::InvertGamepadPitch, true));
    EXPECT_TRUE(state.is_dirty(GameSettingCategory::Controls));
    state.reset_category(GameSettingCategory::Controls);
    EXPECT_FALSE(state.pending().controls.invert_gamepad_pitch);
}

TEST(GameSettings, GraphicsPresetUpdatesOnlyQualityGroups) {
    GameSettingsState applied;
    applied.video.resolution_scale = 73.f;
    applied.video.motion_blur = true;
    GameSettingsEditState state;
    state.begin(applied, applied);
    ASSERT_TRUE(state.set_setting(GameSetting::OverallQuality, GameGraphicsPreset::High));
    auto expected{applied};
    expected.video.overall_quality = GameGraphicsPreset::High;
    expected.video.view_distance_quality = GameQualityLevel::High;
    expected.video.aa_quality = GameQualityLevel::High;
    expected.video.shadow_quality = GameQualityLevel::High;
    expected.video.global_illumination_quality = GameQualityLevel::High;
    expected.video.reflections_quality = GameQualityLevel::High;
    expected.video.post_processing_quality = GameQualityLevel::High;
    expected.video.texture_quality = GameQualityLevel::High;
    expected.video.effects_quality = GameQualityLevel::High;
    expected.video.shading_quality = GameQualityLevel::High;
    EXPECT_EQ(state.pending(), expected);
    ASSERT_TRUE(state.set_setting(GameSetting::ShadowQuality, GameQualityLevel::Medium));
    expected.video.shadow_quality = GameQualityLevel::Medium;
    expected.video.overall_quality = GameGraphicsPreset::Custom;
    EXPECT_EQ(state.pending(), expected);
    ASSERT_TRUE(state.set_setting(GameSetting::OverallQuality, GameGraphicsPreset::Custom));
    EXPECT_EQ(state.pending(), expected);
}

TEST(GameSettings, NormalizesRangesAndChecksSuppliedCapabilities) {
    GameSettingsState state;
    EXPECT_FALSE(game_setting_is_available(GameSettingAvailabilityProvider::AAQuality, state));
    state.video.aa_method = GameAntiAliasingMethod::TAA;
    EXPECT_TRUE(game_setting_is_available(GameSettingAvailabilityProvider::AAQuality, state));
    auto const& volume{game_setting_descriptor(GameSetting::MasterVolume)};
    EXPECT_EQ(normalize_game_setting_value(volume, 4.f), GameSettingValue{1.f});
    EXPECT_FALSE(normalize_game_setting_value(volume, true));
    EXPECT_FALSE(normalize_game_setting_value(volume, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_EQ(game_audio_volume(std::numeric_limits<float>::quiet_NaN()), 1.f);
    EXPECT_EQ(game_audio_volume(-1.f), 0.f);
    EXPECT_EQ(game_audio_volume(3.f), 1.f);
    EXPECT_FALSE(normalize_game_setting_value(game_setting_descriptor(GameSetting::AAQuality),
                                              static_cast<GameQualityLevel>(100)));
    std::array<GameSettingValue, 2> const supported{ScreenResolution{1920, 1080},
                                                    ScreenResolution{1280, 720}};
    auto const& resolution{game_setting_descriptor(GameSetting::Resolution)};
    EXPECT_TRUE(normalize_game_setting_value(resolution, supported.front(), supported, true));
    EXPECT_FALSE(normalize_game_setting_value(resolution, ScreenResolution{1, 1}, supported, true));
    std::array<ScreenResolution, 4> const resolutions{
        {{1920, 1080}, {1280, 720}, {1920, 1080}, {0, 0}}};
    auto const choices{
        game_setting_choices(GameSettingOptionProvider::SupportedResolutions, resolutions)};
    ASSERT_EQ(choices.size(), 2);
    EXPECT_EQ(choices[0].value, (GameSettingValue{ScreenResolution{1280, 720}}));
    EXPECT_EQ(choices[1].value, (GameSettingValue{ScreenResolution{1920, 1080}}));
    EXPECT_EQ(game_setting_descriptor(GameSetting::MouseTurnSensitivity).device,
              GameSettingDevice::KeyboardMouse);
    EXPECT_EQ(game_setting_descriptor(GameSetting::GamepadTurnDeadZone).device,
              GameSettingDevice::Controller);
}

TEST(GameSettings, BindingIdentityIgnoresProfileAndDistinguishesSlotsAndDevices) {
    ControlBindingAddress first{1, FlightMapping::FlightFirePrimaryKeyboardMouse, 1};
    auto second{first};
    second.profile_index = 2;
    EXPECT_EQ(control_binding_identity(first, 1), control_binding_identity(second, 1));
    second.slot = 2;
    EXPECT_NE(control_binding_identity(first, 1), control_binding_identity(second, 1));
    EXPECT_NE(control_binding_identity(first, 1), control_binding_identity(first, 2));
    EXPECT_TRUE(control_binding_matches_device(1, 0, 0));
    EXPECT_TRUE(control_binding_matches_device(1, 1, 0));
    EXPECT_FALSE(control_binding_matches_device(1, 2, 0));
}

TEST(GameSettings, BindingConflictsRespectHardwareProfileAndChord) {
    std::array<ControlBindingSnapshot, 5> bindings{{
        {.address = {1, FlightMapping::FlightFirePrimaryKeyboardMouse, 1},
         .current_key = PhysicalKey::SpaceBar},
        {.address = {1, FlightMapping::FlightBoostKeyboardMouse, 1},
         .current_key = PhysicalKey::SpaceBar},
        {.address = {1, FlightMapping::FlightBrakeKeyboardMouse, 1},
         .current_key = PhysicalKey::SpaceBar},
        {.address = {2, FlightMapping::FlightBoostKeyboardMouse, 1},
         .current_key = PhysicalKey::SpaceBar},
        {.address = {1, FlightMapping::FlightBoostGamepad, 1},
         .current_key = PhysicalKey::SpaceBar},
    }};
    EXPECT_EQ(control_binding_conflicts(bindings, bindings[0].address, PhysicalKey::SpaceBar),
              (std::vector<std::size_t>{1, 2}));
    bindings[0].chord = ControlBindingAddress{1, FlightMapping::FlightSteerKeyboardMouse, 1};
    EXPECT_TRUE(
        control_binding_conflicts(bindings, bindings[0].address, PhysicalKey::SpaceBar).empty());
    bindings[2].chord = bindings[0].chord;
    EXPECT_EQ(control_chord_binding_conflicts(
                  bindings, bindings[0].address, PhysicalKey::SpaceBar, PhysicalKey::SpaceBar),
              (std::vector<std::size_t>{1, 2}));
    auto reordered{bindings};
    std::swap(reordered.front(), reordered.back());
    EXPECT_FALSE(control_bindings_changed(reordered, bindings));
    reordered[0].current_key = PhysicalKey::A;
    EXPECT_TRUE(control_bindings_changed(reordered, bindings));
}

TEST(GameSettings, BindingConflictsReplaceOnlyTheOtherMapping) {
    GameSettingsEditState draft;
    draft.begin({}, {});
    using enum FlightMapping;
    auto const key{draft.pending().controls.bindings[FlightFirePrimaryKeyboardMouse]};
    EXPECT_FALSE(draft.set_binding(FlightBoostKeyboardMouse, key, false));
    EXPECT_EQ(draft.binding_conflicts(FlightBoostKeyboardMouse, key),
              (std::vector{FlightFirePrimaryKeyboardMouse}));
    EXPECT_TRUE(draft.set_binding(FlightBoostKeyboardMouse, key, true));
    EXPECT_EQ(draft.pending().controls.bindings[FlightFirePrimaryKeyboardMouse],
              PhysicalKey::Unbound);
    EXPECT_EQ(draft.pending().controls.bindings[FlightBoostKeyboardMouse], key);
    EXPECT_TRUE(draft.is_dirty(GameSettingCategory::Controls));
    draft.cancel();
    EXPECT_FALSE(draft.is_dirty());
}
}
