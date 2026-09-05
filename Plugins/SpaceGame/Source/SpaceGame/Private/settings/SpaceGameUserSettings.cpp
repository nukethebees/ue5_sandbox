#include "SpaceGame/settings/SpaceGameUserSettings.h"

#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#include "SceneUtils.h"

namespace ml::ioj {
namespace {

void set_console_variable(TCHAR const* name, int32 const value) {
    auto* variable{IConsoleManager::Get().FindConsoleVariable(name)};
    if (variable == nullptr) {
        UE_LOG(LogTemp,
               Warning,
               TEXT("Could not apply game setting: console variable %s is missing"),
               name);
        return;
    }
    variable->Set(value, ECVF_SetByGameOverride);
}

auto engine_anti_aliasing_method(EGameAntiAliasingMethod const method) -> int32 {
    switch (method) {
        case EGameAntiAliasingMethod::FXAA: {
            return AAM_FXAA;
        }
        case EGameAntiAliasingMethod::TAA: {
            return AAM_TemporalAA;
        }
        case EGameAntiAliasingMethod::TSR: {
            return AAM_TSR;
        }
        case EGameAntiAliasingMethod::SMAA: {
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
    if (anti_aliasing_method() == EGameAntiAliasingMethod::SMAA) {
        set_console_variable(TEXT("r.SMAA.Quality"), ScalabilityQuality.AntiAliasingQuality);
    }
    preview_master_volume();
}

void USpaceGameUserSettings::SetToDefaults() {
    Super::SetToDefaults();

    anti_aliasing_method_ = static_cast<int32>(EGameAntiAliasingMethod::Off);
    master_volume_ = 1.0f;
    music_volume_ = 1.0f;
    sfx_volume_ = 1.0f;
    ui_volume_ = 1.0f;
    bees_ = 0;
}

auto USpaceGameUserSettings::anti_aliasing_method() const -> EGameAntiAliasingMethod {
    return static_cast<EGameAntiAliasingMethod>(
        FMath::Clamp(anti_aliasing_method_,
                     static_cast<int32>(EGameAntiAliasingMethod::Off),
                     static_cast<int32>(EGameAntiAliasingMethod::SMAA)));
}

void USpaceGameUserSettings::set_anti_aliasing_method(EGameAntiAliasingMethod const value) {
    anti_aliasing_method_ = static_cast<int32>(value);
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
    master_volume_ = FMath::Clamp(value, 0.0f, 1.0f);
}

void USpaceGameUserSettings::set_music_volume(float const value) {
    music_volume_ = FMath::Clamp(value, 0.0f, 1.0f);
}

void USpaceGameUserSettings::set_sfx_volume(float const value) {
    sfx_volume_ = FMath::Clamp(value, 0.0f, 1.0f);
}

void USpaceGameUserSettings::set_ui_volume(float const value) {
    ui_volume_ = FMath::Clamp(value, 0.0f, 1.0f);
}

auto USpaceGameUserSettings::bees() const -> int32 {
    return bees_;
}

void USpaceGameUserSettings::set_bees(int32 const value) {
    bees_ = FMath::Clamp(value, 0, 5);
}

void USpaceGameUserSettings::preview_master_volume() const {
    if (GEngine == nullptr) {
        UE_LOG(LogTemp, Warning, TEXT("Could not preview master volume: engine is unavailable"));
        return;
    }
    auto audio_device{GEngine->GetMainAudioDevice()};
    if (!audio_device.IsValid()) {
        UE_LOG(
            LogTemp, Warning, TEXT("Could not preview master volume: audio device is unavailable"));
        return;
    }
    audio_device->SetTransientPrimaryVolume(master_volume_);
}

} // namespace ml::ioj
