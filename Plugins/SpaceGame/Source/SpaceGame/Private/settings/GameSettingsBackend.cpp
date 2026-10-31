#include "SpaceGame/settings/GameSettingsBackend.h"

#include "SpaceGame/input/SpaceGameInputUserSettings.h"
#include "SpaceGame/settings/SpaceGameUserSettings.h"
#include <ioj/settings/game_settings_edit_state.h>

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetSystemLibrary.h"

namespace ioj {
namespace {

auto to_game_window_mode(EWindowMode::Type const value) -> GameWindowMode {
    switch (value) {
        case EWindowMode::Fullscreen: {
            return GameWindowMode::Fullscreen;
        }
        case EWindowMode::WindowedFullscreen: {
            return GameWindowMode::Borderless;
        }
        default: {
            return GameWindowMode::Windowed;
        }
    }
}

auto to_engine_window_mode(GameWindowMode const value) -> EWindowMode::Type {
    switch (value) {
        case GameWindowMode::Fullscreen: {
            return EWindowMode::Fullscreen;
        }
        case GameWindowMode::Borderless: {
            return EWindowMode::WindowedFullscreen;
        }
        default: {
            return EWindowMode::Windowed;
        }
    }
}

auto to_quality(int32 const value) -> GameQualityLevel {
    return static_cast<GameQualityLevel>(FMath::Clamp(value, 0, 3));
}

auto read_settings(USpaceGameUserSettings& settings,
                   USpaceGameInputUserSettings const& input,
                   FIntPoint const resolution) -> GameSettingsState {
    float normalized_scale{};
    float resolution_scale{};
    float minimum_scale{};
    float maximum_scale{};
    settings.GetResolutionScaleInformationEx(
        normalized_scale, resolution_scale, minimum_scale, maximum_scale);

    GameSettingsState result{
        .resolution = {resolution.X, resolution.Y},
        .window_mode = to_game_window_mode(settings.GetFullscreenMode()),
        .vsync = settings.IsVSyncEnabled(),
        .frame_rate_limit = settings.GetFrameRateLimit(),
        .resolution_scale = resolution_scale,
        .view_distance_quality = to_quality(settings.GetViewDistanceQuality()),
        .aa_method = settings.anti_aliasing_method(),
        .aa_quality = to_quality(settings.GetAntiAliasingQuality()),
        .shadow_quality = to_quality(settings.GetShadowQuality()),
        .global_illumination_quality = to_quality(settings.GetGlobalIlluminationQuality()),
        .reflections_quality = to_quality(settings.GetReflectionQuality()),
        .post_processing_quality = to_quality(settings.GetPostProcessingQuality()),
        .texture_quality = to_quality(settings.GetTextureQuality()),
        .effects_quality = to_quality(settings.GetVisualEffectQuality()),
        .shading_quality = to_quality(settings.GetShadingQuality()),
        .bloom = settings.bloom_enabled(),
        .motion_blur = settings.motion_blur_enabled(),
        .master_volume = settings.master_volume(),
        .music_volume = settings.music_volume(),
        .sfx_volume = settings.sfx_volume(),
        .ui_volume = settings.ui_volume(),
        .bees = settings.bees(),
        .mouse_turn_sensitivity = input.mouse_turn_sensitivity(),
        .gamepad_turn_sensitivity = input.gamepad_turn_sensitivity(),
        .gamepad_turn_dead_zone = input.gamepad_turn_dead_zone(),
        .gamepad_move_dead_zone = input.gamepad_move_dead_zone(),
        .invert_mouse_pitch = input.invert_mouse_pitch(),
        .invert_mouse_yaw = input.invert_mouse_yaw(),
        .invert_gamepad_pitch = input.invert_gamepad_pitch(),
        .invert_gamepad_yaw = input.invert_gamepad_yaw(),
        .invert_gamepad_roll = input.invert_gamepad_roll(),
        .invert_gamepad_vertical_translation = input.invert_gamepad_vertical_translation(),
    };
    result.overall_quality = graphics_preset(result);
    return result;
}

} // namespace

void FGameSettingsBackend::set_local_player(ULocalPlayer* const local_player) {
    local_player_ = local_player;
}

auto FGameSettingsBackend::read() const -> GameSettingsState {
    auto* const settings{user_settings()};
    if (settings == nullptr) {
        return {};
    }
    auto const* input{input_user_settings()};
    if (input == nullptr) {
        input = GetDefault<USpaceGameInputUserSettings>();
    }
    return read_settings(*settings, *input, settings->GetScreenResolution());
}

auto FGameSettingsBackend::defaults() const -> GameSettingsState {
    auto* defaults{NewObject<USpaceGameUserSettings>()};
    defaults->SetToDefaults();

    auto* current{user_settings()};
    auto resolution{current != nullptr ? current->GetDesktopResolution() : FIntPoint{1920, 1080}};
    if (resolution.X <= 0 || resolution.Y <= 0) {
        resolution = FIntPoint{1920, 1080};
    }

    return read_settings(*defaults, *GetDefault<USpaceGameInputUserSettings>(), resolution);
}

void FGameSettingsBackend::preview_immediate(GameSettingsState const& state,
                                             GameSetting const setting) const {
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return;
    }
    switch (setting) {
        case GameSetting::MasterVolume: {
            settings->set_master_volume(state.master_volume);
            break;
        }
        case GameSetting::MusicVolume: {
            settings->set_music_volume(state.music_volume);
            break;
        }
        case GameSetting::SfxVolume: {
            settings->set_sfx_volume(state.sfx_volume);
            break;
        }
        case GameSetting::UIVolume: {
            settings->set_ui_volume(state.ui_volume);
            break;
        }
        case GameSetting::Bees: {
            settings->set_bees(state.bees);
            break;
        }
        case GameSetting::MouseTurnSensitivity:
        case GameSetting::GamepadTurnSensitivity:
        case GameSetting::GamepadTurnDeadZone:
        case GameSetting::GamepadMoveDeadZone:
        case GameSetting::InvertMousePitch:
        case GameSetting::InvertMouseYaw:
        case GameSetting::InvertGamepadPitch:
        case GameSetting::InvertGamepadYaw:
        case GameSetting::InvertGamepadRoll:
        case GameSetting::InvertGamepadVerticalTranslation: {
            apply_input_settings(state);
            break;
        }
        default: {
            break;
        }
    }
}

void FGameSettingsBackend::apply_non_display(GameSettingsState const& state) const {
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return;
    }
    write_non_display(*settings, state);
    settings->ApplyNonResolutionSettings();
    apply_input_settings(state);
}

void FGameSettingsBackend::apply_input_settings(GameSettingsState const& state) const {
    if (auto* const input_settings{input_user_settings()}) {
        input_settings->set_mouse_turn_sensitivity(state.mouse_turn_sensitivity);
        input_settings->set_gamepad_turn_sensitivity(state.gamepad_turn_sensitivity);
        input_settings->set_gamepad_turn_dead_zone(state.gamepad_turn_dead_zone);
        input_settings->set_gamepad_move_dead_zone(state.gamepad_move_dead_zone);
        input_settings->set_invert_mouse_pitch(state.invert_mouse_pitch);
        input_settings->set_invert_mouse_yaw(state.invert_mouse_yaw);
        input_settings->set_invert_gamepad_pitch(state.invert_gamepad_pitch);
        input_settings->set_invert_gamepad_yaw(state.invert_gamepad_yaw);
        input_settings->set_invert_gamepad_roll(state.invert_gamepad_roll);
        input_settings->set_invert_gamepad_vertical_translation(
            state.invert_gamepad_vertical_translation);
        input_settings->ApplySettings();
    }
}

void FGameSettingsBackend::apply_display(GameSettingsState const& state) const {
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return;
    }
    settings->SetScreenResolution({state.resolution.width, state.resolution.height});
    settings->SetFullscreenMode(to_engine_window_mode(state.window_mode));
    settings->ApplyResolutionSettings(false);
}

void FGameSettingsBackend::confirm_display() const {
    if (auto* settings{user_settings()}) {
        settings->ConfirmVideoMode();
    }
}

void FGameSettingsBackend::revert_display() const {
    if (auto* settings{user_settings()}) {
        settings->RevertVideoMode();
        settings->ApplyResolutionSettings(false);
    }
}

void FGameSettingsBackend::save() const {
    if (auto* settings{user_settings()}) {
        settings->SaveSettings();
    }
}

auto FGameSettingsBackend::options(GameSettingOptionProvider const provider) const
    -> TArray<FGameSettingOption> {
    std::vector<ScreenResolution> resolutions;
    if (provider == GameSettingOptionProvider::SupportedResolutions) {
        TArray<FIntPoint> available;
        UKismetSystemLibrary::GetSupportedFullscreenResolutions(available);
        resolutions.reserve(available.Num());
        for (auto const value : available) {
            resolutions.push_back({value.X, value.Y});
        }
    }
    auto const choices{game_setting_choices(provider, resolutions)};
    TArray<FGameSettingOption> result;
    result.Reserve(static_cast<int32>(choices.size()));
    for (auto const& choice : choices) {
        auto const* number{std::get_if<float>(&choice.value)};
        auto label{provider == GameSettingOptionProvider::FrameRateLimits && number != nullptr &&
                           *number > 0.f
                       ? FText::AsNumber(static_cast<int32>(*number))
                       : FText::FromString(UTF8_TO_TCHAR(choice.label.c_str()))};
        result.Add({choice.value, MoveTemp(label)});
    }
    return result;
}

auto FGameSettingsBackend::user_settings() const -> USpaceGameUserSettings* {
    if (GEngine == nullptr) {
        UE_LOG(LogTemp, Error, TEXT("Game settings are unavailable because the engine is null"));
        return nullptr;
    }
    auto* result{Cast<USpaceGameUserSettings>(GEngine->GetGameUserSettings())};
    if (result == nullptr) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Configured GameUserSettings class is not USpaceGameUserSettings"));
    }
    return result;
}

auto FGameSettingsBackend::input_user_settings() const -> USpaceGameInputUserSettings* {
    auto* const local_player{local_player_};
    if (!IsValid(local_player)) {
        return nullptr;
    }
    auto* const subsystem{
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(local_player)};
    if (!IsValid(subsystem)) {
        UE_LOG(LogTemp, Error, TEXT("Enhanced Input local-player subsystem is unavailable"));
        return nullptr;
    }
    auto* const result{Cast<USpaceGameInputUserSettings>(subsystem->GetUserSettings())};
    if (!IsValid(result)) {
        UE_LOG(LogTemp,
               Error,
               TEXT("Configured Enhanced Input settings class is not "
                    "USpaceGameInputUserSettings"));
    }
    return result;
}

void FGameSettingsBackend::write_non_display(USpaceGameUserSettings& settings,
                                             GameSettingsState const& state) const {
    settings.SetVSyncEnabled(state.vsync);
    settings.SetFrameRateLimit(state.frame_rate_limit);
    settings.SetResolutionScaleValueEx(state.resolution_scale);
    settings.set_anti_aliasing_method(state.aa_method);
    settings.SetViewDistanceQuality(static_cast<int32>(state.view_distance_quality));
    settings.SetAntiAliasingQuality(static_cast<int32>(state.aa_quality));
    settings.SetShadowQuality(static_cast<int32>(state.shadow_quality));
    settings.SetGlobalIlluminationQuality(static_cast<int32>(state.global_illumination_quality));
    settings.SetReflectionQuality(static_cast<int32>(state.reflections_quality));
    settings.SetPostProcessingQuality(static_cast<int32>(state.post_processing_quality));
    settings.SetTextureQuality(static_cast<int32>(state.texture_quality));
    settings.SetVisualEffectQuality(static_cast<int32>(state.effects_quality));
    settings.SetShadingQuality(static_cast<int32>(state.shading_quality));
    settings.set_bloom_enabled(state.bloom);
    settings.set_motion_blur_enabled(state.motion_blur);
    settings.set_master_volume(state.master_volume);
    settings.set_music_volume(state.music_volume);
    settings.set_sfx_volume(state.sfx_volume);
    settings.set_ui_volume(state.ui_volume);
    settings.set_bees(state.bees);
}

} // namespace ioj
