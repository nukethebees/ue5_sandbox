#include "SpaceGame/settings/SpaceGameUserSettings.h"

#include <ioj/settings/game_settings_edit_state.h>

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

auto USpaceGameUserSettings::anti_aliasing_method() const -> GameAntiAliasingMethod {
    return anti_aliasing_method_;
}

void USpaceGameUserSettings::set_anti_aliasing_method(GameAntiAliasingMethod const value) {
    anti_aliasing_method_ = value;
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

} // namespace ioj
