#include <ioj/settings/control_bindings.h>
#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/sim/player/flight_model_settings_codec.h>
#include <ioj/sim/player/game_settings_session.h>

#include <gtest/gtest.h>

#include <array>
#include <limits>

namespace ioj::tests {
TEST(GameSettings, TracksCategoryResetCommitAndCancellation) {
    GameSettingsState applied;
    applied.master_volume = 0.25f;
    applied.bees = 2;
    auto defaults{applied};
    defaults.vsync = true;
    defaults.master_volume = 1.f;
    GameSettingsEditState state;
    state.begin(applied, defaults);
    EXPECT_FALSE(state.is_dirty());
    ASSERT_TRUE(state.set_setting(GameSetting::MasterVolume, 0.5f));
    EXPECT_TRUE(state.is_dirty(GameSettingCategory::Audio));
    EXPECT_FALSE(state.is_dirty(GameSettingCategory::Video));
    state.reset_category(GameSettingCategory::Video);
    EXPECT_TRUE(state.is_at_defaults(GameSettingCategory::Video));
    EXPECT_EQ(state.pending().master_volume, 0.5f);
    EXPECT_TRUE(state.is_dirty(GameSettingCategory::Video));
    state.commit_all();
    EXPECT_FALSE(state.is_dirty());
    ASSERT_TRUE(state.set_setting(GameSetting::Bees, std::int32_t{4}));
    state.cancel();
    EXPECT_EQ(state.pending().bees, 2);
    ASSERT_TRUE(state.set_setting(GameSetting::InvertGamepadPitch, true));
    EXPECT_TRUE(state.is_dirty(GameSettingCategory::Controls));
    state.reset_category(GameSettingCategory::Controls);
    EXPECT_FALSE(state.pending().invert_gamepad_pitch);
}

TEST(GameSettings, GraphicsPresetUpdatesOnlyQualityGroups) {
    GameSettingsState applied;
    applied.resolution_scale = 73.f;
    applied.motion_blur = true;
    GameSettingsEditState state;
    state.begin(applied, applied);
    ASSERT_TRUE(state.set_setting(GameSetting::OverallQuality, GameGraphicsPreset::High));
    auto expected{applied};
    expected.overall_quality = GameGraphicsPreset::High;
    expected.view_distance_quality = GameQualityLevel::High;
    expected.aa_quality = GameQualityLevel::High;
    expected.shadow_quality = GameQualityLevel::High;
    expected.global_illumination_quality = GameQualityLevel::High;
    expected.reflections_quality = GameQualityLevel::High;
    expected.post_processing_quality = GameQualityLevel::High;
    expected.texture_quality = GameQualityLevel::High;
    expected.effects_quality = GameQualityLevel::High;
    expected.shading_quality = GameQualityLevel::High;
    EXPECT_EQ(state.pending(), expected);
    ASSERT_TRUE(state.set_setting(GameSetting::ShadowQuality, GameQualityLevel::Medium));
    expected.shadow_quality = GameQualityLevel::Medium;
    expected.overall_quality = GameGraphicsPreset::Custom;
    EXPECT_EQ(state.pending(), expected);
    ASSERT_TRUE(state.set_setting(GameSetting::OverallQuality, GameGraphicsPreset::Custom));
    EXPECT_EQ(state.pending(), expected);
}

TEST(GameSettings, NormalizesRangesAndChecksSuppliedCapabilities) {
    GameSettingsState state;
    EXPECT_FALSE(game_setting_is_available(GameSettingAvailabilityProvider::AAQuality, state));
    state.aa_method = GameAntiAliasingMethod::TAA;
    EXPECT_TRUE(game_setting_is_available(GameSettingAvailabilityProvider::AAQuality, state));
    auto const& volume{game_setting_descriptor(GameSetting::MasterVolume)};
    EXPECT_EQ(normalize_game_setting_value(volume, 4.f), GameSettingValue{1.f});
    EXPECT_FALSE(normalize_game_setting_value(volume, std::int32_t{4}));
    EXPECT_FALSE(normalize_game_setting_value(volume, std::numeric_limits<float>::quiet_NaN()));
    EXPECT_EQ(game_audio_volume(std::numeric_limits<float>::quiet_NaN()), 1.f);
    EXPECT_EQ(game_audio_volume(-1.f), 0.f);
    EXPECT_EQ(game_audio_volume(3.f), 1.f);
    EXPECT_EQ(game_bees_count(-2), 0);
    EXPECT_EQ(game_bees_count(6), 5);
    EXPECT_EQ(game_anti_aliasing_method(-1), GameAntiAliasingMethod::Off);
    EXPECT_EQ(game_anti_aliasing_method(100), GameAntiAliasingMethod::SMAA);
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
    ControlBindingAddress first{"one", "fire", "keyboard", 1};
    auto second{first};
    second.profile_id = "two";
    EXPECT_EQ(control_binding_identity(first, 1), control_binding_identity(second, 1));
    second.slot = 2;
    EXPECT_NE(control_binding_identity(first, 1), control_binding_identity(second, 1));
    EXPECT_NE(control_binding_identity(first, 1), control_binding_identity(first, 2));
    EXPECT_TRUE(control_binding_matches_device(1, 0, 0));
    EXPECT_TRUE(control_binding_matches_device(1, 1, 0));
    EXPECT_FALSE(control_binding_matches_device(1, 2, 0));
}

TEST(GameSettings, BindingConflictsRespectHardwareProfileScopeAndChord) {
    std::array<ControlBindingSnapshot, 5> bindings{{
        {.address = {"one", "fire", "keyboard", 1}, .current_key = "space", .scope = 1},
        {.address = {"one", "boost", "keyboard", 1}, .current_key = "space", .scope = 1},
        {.address = {"one", "brake", "keyboard", 1}, .current_key = "space", .scope = 2},
        {.address = {"two", "boost", "keyboard", 1}, .current_key = "space", .scope = 1},
        {.address = {"one", "boost", "gamepad", 1}, .current_key = "space", .scope = 1},
    }};
    EXPECT_EQ(control_binding_conflicts(bindings, bindings[0].address, "space"),
              std::vector<std::size_t>{1});
    bindings[0].scope.reset();
    EXPECT_EQ(control_binding_conflicts(bindings, bindings[0].address, "space"),
              (std::vector<std::size_t>{1, 2}));
    bindings[0].chord = ControlBindingAddress{"one", "modifier", "keyboard", 1};
    EXPECT_TRUE(control_binding_conflicts(bindings, bindings[0].address, "space").empty());
    bindings[2].chord = bindings[0].chord;
    EXPECT_EQ(control_chord_binding_conflicts(bindings, bindings[0].address, "space", "space"),
              (std::vector<std::size_t>{1, 2}));
    auto reordered{bindings};
    std::swap(reordered.front(), reordered.back());
    EXPECT_FALSE(control_bindings_changed(reordered, bindings));
    reordered[0].current_key = "other";
    EXPECT_TRUE(control_bindings_changed(reordered, bindings));
}

TEST(GameSettings, DisplayRevertKeepsAppliedControlsAndFlightModel) {
    GameSettingsState applied;
    applied.resolution = {1920, 1080};
    applied.mouse_turn_sensitivity = 1.f;
    auto const loadout{sim::player::make_default_flight_model_loadout()};
    GameSettingsSession session;
    session.initialise(applied, applied, loadout);
    EXPECT_FALSE(session.is_editing());
    EXPECT_FALSE(session.is_awaiting_display_confirmation());
    EXPECT_FALSE(session.set_setting(GameSetting::VSync, true));
    ASSERT_TRUE(session.begin_edit(applied, applied));
    EXPECT_TRUE(session.is_editing());
    EXPECT_FALSE(session.is_awaiting_display_confirmation());
    ASSERT_TRUE(session.set_setting(GameSetting::Resolution, ScreenResolution{1280, 720}));
    ASSERT_TRUE(session.set_setting(GameSetting::MouseTurnSensitivity, 1.5f));
    auto profile{loadout.profiles[0]};
    profile.config.translation.forward.passive_drag =
        NonNegativeFinite<float>::make(profile.config.translation.forward.passive_drag + 1.f);
    ASSERT_TRUE(session.set_flight_model_profile(sim::player::FlightModelSlot::First, profile));
    EXPECT_EQ(session.apply(10.0), GameSettingsApplyResult::ConfirmDisplay);
    EXPECT_TRUE(session.is_editing());
    EXPECT_TRUE(session.is_awaiting_display_confirmation());
    EXPECT_FALSE(session.begin_edit(applied, applied));
    EXPECT_EQ(session.display_confirmation_seconds_remaining(10.25), 15);
    EXPECT_FALSE(session.display_confirmation_expired(24.99));
    EXPECT_TRUE(session.display_confirmation_expired(25.0));
    EXPECT_FALSE(session.cancel());
    EXPECT_FALSE(session.set_setting(GameSetting::VSync, true));
    ASSERT_TRUE(session.revert_display());
    EXPECT_TRUE(session.is_editing());
    EXPECT_FALSE(session.is_awaiting_display_confirmation());
    EXPECT_EQ(session.settings_state().resolution, applied.resolution);
    EXPECT_EQ(session.settings_state().mouse_turn_sensitivity, 1.5f);
    EXPECT_TRUE(session.flight_model_loadout().profiles[0].customized);
    EXPECT_FALSE(session.is_dirty());
    ASSERT_TRUE(session.set_setting(GameSetting::Resolution, ScreenResolution{1280, 720}));
    EXPECT_EQ(session.apply(30.0), GameSettingsApplyResult::ConfirmDisplay);
    ASSERT_TRUE(session.confirm_display());
    EXPECT_EQ(session.applied_state().resolution, (ScreenResolution{1280, 720}));
    EXPECT_FALSE(session.is_awaiting_display_confirmation());
    ASSERT_TRUE(session.cancel());
    EXPECT_FALSE(session.is_editing());
    EXPECT_FALSE(session.confirm_display());
    EXPECT_FALSE(session.revert_display());
    EXPECT_EQ(session.apply(40.0), GameSettingsApplyResult::Ignored);
    EXPECT_EQ(session.settings_state().resolution, (ScreenResolution{1280, 720}));
}

TEST(GameSettings, FlightModelCodecRejectsCorruptionWithoutChangingOutput) {
    auto original{sim::player::make_flight_model_profile(sim::player::FlightModelPreset::Gunship)};
    original.config.translation.up.passive_drag = NonNegativeFinite<float>{123.f};
    original.customized = true;
    auto const encoded{flight_model_settings_codec::encode(original)};
    auto profile{sim::player::make_flight_model_profile(sim::player::FlightModelPreset::Starfox)};
    ASSERT_TRUE(flight_model_settings_codec::decode(encoded, profile));
    EXPECT_EQ(profile, original);
    for (auto const& invalid : {std::string{"broken"},
                                encoded.substr(0, encoded.size() - 2),
                                encoded + "00",
                                std::string{"flight-model:1:"} + encoded.substr(15)}) {
        EXPECT_FALSE(flight_model_settings_codec::decode(invalid, profile));
        EXPECT_EQ(profile, original);
    }
    struct Corruption {
        std::size_t byte_offset;
        std::string_view hex;
    };
    for (auto const corruption : {
             Corruption{0, "ff"},
             Corruption{2, "ff"},
             Corruption{9, "ff"},
             Corruption{5, "00000040"},
             Corruption{10, "000080bf"},
             Corruption{10, "0000807f"},
             Corruption{10, "0000c07f"},
             Corruption{18, "00000000"},
             Corruption{22, "0000803f"},
         }) {
        auto invalid{encoded};
        invalid.replace(std::string_view{"flight-model:2:"}.size() + corruption.byte_offset * 2,
                        corruption.hex.size(),
                        corruption.hex);
        EXPECT_FALSE(flight_model_settings_codec::decode(invalid, profile));
        EXPECT_EQ(profile, original);
    }

    auto const loadout{flight_model_settings_codec::decode_loadout({"", "", encoded, encoded})};
    EXPECT_TRUE(loadout.invalid_slots[2]);
    EXPECT_FALSE(loadout.invalid_slots[3]);
    EXPECT_EQ(loadout.value.profiles[2],
              sim::player::make_flight_model_profile(sim::player::FlightModelPreset::Skater));
    EXPECT_EQ(loadout.value.profiles[3], original);
    auto changed_topology{original};
    changed_topology.config.translation.up.manual.semantic =
        sim::player::TranslationSemantic::Disabled;
    auto const rejected{flight_model_settings_codec::decode_loadout(
        {"", "", "", flight_model_settings_codec::encode(changed_topology)})};
    EXPECT_TRUE(rejected.invalid_slots[3]);
    EXPECT_EQ(rejected.value.profiles[3],
              sim::player::make_flight_model_profile(sim::player::FlightModelPreset::Gunship));
}
}
