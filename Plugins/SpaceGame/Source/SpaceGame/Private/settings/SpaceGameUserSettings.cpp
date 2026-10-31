#include "SpaceGame/settings/SpaceGameUserSettings.h"

#include <ioj/settings/game_settings_edit_state.h>
#include <ioj/sim/player/flight_model_settings_codec.h>

#include "HAL/IConsoleManager.h"
#include "SceneUtils.h"

namespace ioj {
namespace {

auto console_variable(TCHAR const* name) -> IConsoleVariable* {
    auto* variable{IConsoleManager::Get().FindConsoleVariable(name)};
    if (variable == nullptr) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not apply game setting: console variable %s is missing"),
               name);
    }
    return variable;
}

void set_console_variable(TCHAR const* name, int32 const value) {
    auto* variable{console_variable(name)};
    if (variable == nullptr) {
        return;
    }
    variable->Set(value, ECVF_SetByGameOverride);
}

void apply_rendering_effect(TCHAR const* default_feature_name,
                            TCHAR const* quality_name,
                            bool const enabled) {
    set_console_variable(default_feature_name, enabled ? 1 : 0);

    auto* quality{console_variable(quality_name)};
    if (quality == nullptr) {
        return;
    }

    quality->Unset(ECVF_SetByGameOverride);
    if (!enabled || quality->GetInt() <= 0) {
        quality->Set(enabled ? 1 : 0, ECVF_SetByGameOverride);
    }
}

auto engine_anti_aliasing_method(GameAntiAliasingMethod const method) -> int32 {
    switch (method) {
        case GameAntiAliasingMethod::FXAA: {
            return AAM_FXAA;
        }
        case GameAntiAliasingMethod::TAA: {
            return AAM_TemporalAA;
        }
        case GameAntiAliasingMethod::TSR: {
            return AAM_TSR;
        }
        case GameAntiAliasingMethod::SMAA: {
            return AAM_SMAA;
        }
        default: {
            return AAM_None;
        }
    }
}

} // namespace

void USpaceGameUserSettings::ApplyNonResolutionSettings() {
    Super::ApplyNonResolutionSettings();

    set_console_variable(TEXT("r.AntiAliasingMethod"),
                         engine_anti_aliasing_method(anti_aliasing_method()));
    if (anti_aliasing_method() == GameAntiAliasingMethod::SMAA) {
        set_console_variable(TEXT("r.SMAA.Quality"), ScalabilityQuality.AntiAliasingQuality);
    }
    apply_rendering_effect(TEXT("r.DefaultFeature.Bloom"), TEXT("r.BloomQuality"), bloom_enabled_);
    apply_rendering_effect(
        TEXT("r.DefaultFeature.MotionBlur"), TEXT("r.MotionBlurQuality"), motion_blur_enabled_);
    IConsoleManager::Get().CallAllConsoleVariableSinks();
}

void USpaceGameUserSettings::ValidateSettings() {
    Super::ValidateSettings();

    set_anti_aliasing_method(game_anti_aliasing_method(anti_aliasing_method_));
    set_master_volume(master_volume_);
    set_music_volume(music_volume_);
    set_sfx_volume(sfx_volume_);
    set_ui_volume(ui_volume_);
    set_bees(bees_);
}

void USpaceGameUserSettings::SetToDefaults() {
    Super::SetToDefaults();

    anti_aliasing_method_ = static_cast<int32>(GameAntiAliasingMethod::Off);
    bloom_enabled_ = true;
    motion_blur_enabled_ = false;
    master_volume_ = 1.0f;
    music_volume_ = 1.0f;
    sfx_volume_ = 1.0f;
    ui_volume_ = 1.0f;
    bees_ = 0;
    starfox_flight_model_.Reset();
    fighter_flight_model_.Reset();
    skater_flight_model_.Reset();
    gunship_flight_model_.Reset();
}

auto USpaceGameUserSettings::anti_aliasing_method() const -> GameAntiAliasingMethod {
    return game_anti_aliasing_method(anti_aliasing_method_);
}

void USpaceGameUserSettings::set_anti_aliasing_method(GameAntiAliasingMethod const value) {
    anti_aliasing_method_ =
        static_cast<int32>(game_anti_aliasing_method(static_cast<int32>(value)));
}

auto USpaceGameUserSettings::bloom_enabled() const -> bool {
    return bloom_enabled_;
}

auto USpaceGameUserSettings::motion_blur_enabled() const -> bool {
    return motion_blur_enabled_;
}

void USpaceGameUserSettings::set_bloom_enabled(bool const value) {
    bloom_enabled_ = value;
}

void USpaceGameUserSettings::set_motion_blur_enabled(bool const value) {
    motion_blur_enabled_ = value;
}

auto USpaceGameUserSettings::master_volume() const -> float {
    return master_volume_;
}

auto USpaceGameUserSettings::music_volume() const -> float {
    return music_volume_;
}

auto USpaceGameUserSettings::sfx_volume() const -> float {
    return sfx_volume_;
}

auto USpaceGameUserSettings::ui_volume() const -> float {
    return ui_volume_;
}

void USpaceGameUserSettings::set_master_volume(float const value) {
    master_volume_ = game_audio_volume(value);
}

void USpaceGameUserSettings::set_music_volume(float const value) {
    music_volume_ = game_audio_volume(value);
}

void USpaceGameUserSettings::set_sfx_volume(float const value) {
    sfx_volume_ = game_audio_volume(value);
}

void USpaceGameUserSettings::set_ui_volume(float const value) {
    ui_volume_ = game_audio_volume(value);
}

auto USpaceGameUserSettings::bees() const -> int32 {
    return bees_;
}

void USpaceGameUserSettings::set_bees(int32 const value) {
    bees_ = game_bees_count(value);
}

auto USpaceGameUserSettings::flight_model_loadout() const -> sim::player::FlightModelLoadout {
    std::array<std::string, 4> const encoded{TCHAR_TO_UTF8(*starfox_flight_model_),
                                             TCHAR_TO_UTF8(*fighter_flight_model_),
                                             TCHAR_TO_UTF8(*skater_flight_model_),
                                             TCHAR_TO_UTF8(*gunship_flight_model_)};
    auto result{flight_model_settings_codec::decode_loadout(
        {encoded[0], encoded[1], encoded[2], encoded[3]})};
    for (auto const invalid : result.invalid_slots) {
        if (invalid) {
            UE_LOG(LogTemp, Warning, TEXT("Ignoring invalid saved flight model slot"));
        }
    }
    return std::move(result.value);
}

void USpaceGameUserSettings::set_flight_model_loadout(
    sim::player::FlightModelLoadout const& loadout) {
    starfox_flight_model_ =
        UTF8_TO_TCHAR(flight_model_settings_codec::encode(loadout.profiles[0]).c_str());
    fighter_flight_model_ =
        UTF8_TO_TCHAR(flight_model_settings_codec::encode(loadout.profiles[1]).c_str());
    skater_flight_model_ =
        UTF8_TO_TCHAR(flight_model_settings_codec::encode(loadout.profiles[2]).c_str());
    gunship_flight_model_ =
        UTF8_TO_TCHAR(flight_model_settings_codec::encode(loadout.profiles[3]).c_str());
}

} // namespace ioj
