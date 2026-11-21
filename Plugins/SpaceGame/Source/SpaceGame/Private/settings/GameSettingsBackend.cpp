#include "SpaceGame/settings/GameSettingsBackend.h"

#include "SpaceGame/settings/SpaceGameUserSettings.h"
#include <ioj/settings/game_settings_edit_state.h>

#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputSubsystems.h"
#include "Kismet/KismetSystemLibrary.h"

namespace ioj {
namespace {

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

} // namespace

void FGameSettingsBackend::apply_non_display(GameSettingsState const& state) const {
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return;
    }
    // Unreal can reload its bootstrap INI while validating an uninitialised settings object.
    settings->ValidateSettings();
    write_non_display(*settings, state);
    settings->ApplyNonResolutionSettings();
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
}

} // namespace ioj
