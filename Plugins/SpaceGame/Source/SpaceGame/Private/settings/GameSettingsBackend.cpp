#include "SpaceGame/settings/GameSettingsBackend.h"
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
#include "GameHost.h"
#include <ioj/host/game_host.h>
#endif

#include "GenericPlatform/GenericWindow.h"
#include "SpaceGame/settings/SpaceGameUserSettings.h"
#include <ioj/settings/game_setting_option_provider.h>
#include <ioj/settings/game_settings_state.h>
#include <ioj/settings/game_window_mode.h>
#include <ioj/settings/launch_display.h>
#include <ioj/settings/player_settings.h>
#include <ioj/settings/screen_resolution.h>

#include "Containers/Array.h"
#include "CoreGlobals.h"
#include "Engine/Engine.h"
#include "HAL/Platform.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Logging/LogMacros.h"
#include "Logging/LogVerbosity.h"
#include "Math/MathFwd.h"
#include "Templates/Casts.h"

#include <expected>
#include <string>
#include <vector>

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

auto FGameSettingsBackend::inspect_display() const -> std::expected<HostDisplay, std::string> {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    auto& host{game_host()};
    HostDisplay display{};
    if (auto error{host.apply_display(host.window, DisplayOperation::Inspect, display)}) {
        return std::expected<HostDisplay, std::string>{std::unexpect, error};
    }
    return display;
#else
    return std::expected<HostDisplay, std::string>{std::unexpect, "PIE does not own a host window"};
#endif
}

auto FGameSettingsBackend::verify_display() const -> SettingsResult {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    auto& host{game_host()};
    HostDisplay display{};
    if (auto error{host.apply_display(host.window, DisplayOperation::Verify, display)}) {
        return SettingsResult{std::unexpect, error};
    }
#endif
    return {};
}

auto FGameSettingsBackend::apply_display(GameSettingsState const& state) const -> SettingsResult {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    auto& host{game_host()};
    HostDisplay display{state.window_mode, state.resolution};

    if (auto error{host.apply_display(host.window, DisplayOperation::Preview, display)}) {
        return SettingsResult{std::unexpect, error};
    }
#else
    auto* settings{user_settings()};
    if (settings == nullptr) {
        return SettingsResult{std::unexpect, "Game user settings are unavailable"};
    }

    settings->SetScreenResolution({state.resolution.width, state.resolution.height});
    settings->SetFullscreenMode(to_engine_window_mode(state.window_mode));
    settings->ApplyResolutionSettings(false);
#endif
    return {};
}

void FGameSettingsBackend::confirm_display() const {
#if !defined(UE_LIBRARY_ENABLED) || !UE_LIBRARY_ENABLED
    if (auto* settings{user_settings()}) {
        settings->ConfirmVideoMode();
    }
#endif
}

auto FGameSettingsBackend::revert_display() const -> SettingsResult {
#if defined(UE_LIBRARY_ENABLED) && UE_LIBRARY_ENABLED
    auto& host{game_host()};
    HostDisplay display{};

    if (auto error{host.apply_display(host.window, DisplayOperation::Revert, display)}) {
        return SettingsResult{std::unexpect, error};
    }
#else
    if (auto* settings{user_settings()}) {
        settings->RevertVideoMode();
        settings->ApplyResolutionSettings(false);
    }
#endif
    return {};
}

auto FGameSettingsBackend::supported_resolutions(GameSettingOptionProvider provider) const
    -> std::vector<ScreenResolution> {
    std::vector<ScreenResolution> result;
    if (provider == GameSettingOptionProvider::SupportedResolutions) {
        TArray<FIntPoint> available;
        UKismetSystemLibrary::GetSupportedFullscreenResolutions(available);
        result.reserve(available.Num());
        for (auto value : available) {
            result.push_back({value.X, value.Y});
        }
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
