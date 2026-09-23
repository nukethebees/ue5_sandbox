#pragma once

#include "GameFramework/GameUserSettings.h"
#include "SpaceGame/settings/GameSettingsTypes.h"

#include <ioj/sim/player/flight_model_config.h>

#include "SpaceGameUserSettings.generated.h"

namespace ml::ioj {

UCLASS(Config = GameUserSettings)
class SPACEGAME_API USpaceGameUserSettings : public UGameUserSettings {
    GENERATED_BODY()
  public:
    virtual void ApplyNonResolutionSettings() override;
    virtual void ValidateSettings() override;
    virtual void SetToDefaults() override;

    auto anti_aliasing_method() const -> EGameAntiAliasingMethod;
    void set_anti_aliasing_method(EGameAntiAliasingMethod value);

    auto bloom_enabled() const -> bool;
    auto motion_blur_enabled() const -> bool;
    void set_bloom_enabled(bool value);
    void set_motion_blur_enabled(bool value);

    auto master_volume() const -> float;
    auto music_volume() const -> float;
    auto sfx_volume() const -> float;
    auto ui_volume() const -> float;
    void set_master_volume(float value);
    void set_music_volume(float value);
    void set_sfx_volume(float value);
    void set_ui_volume(float value);

    auto bees() const -> int32;
    void set_bees(int32 value);

    auto flight_model_loadout() const -> ::ioj::sim::player::FlightModelLoadout;
    void set_flight_model_loadout(::ioj::sim::player::FlightModelLoadout const& loadout);
  private:
    UPROPERTY(Config)
    int32 anti_aliasing_method_{};

    UPROPERTY(Config)
    bool bloom_enabled_{true};

    UPROPERTY(Config)
    bool motion_blur_enabled_{};

    UPROPERTY(Config)
    float master_volume_{1.0f};

    UPROPERTY(Config)
    float music_volume_{1.0f};

    UPROPERTY(Config)
    float sfx_volume_{1.0f};

    UPROPERTY(Config)
    float ui_volume_{1.0f};

    UPROPERTY(Config)
    int32 bees_{};

    UPROPERTY(Config)
    FString starfox_flight_model_{};

    UPROPERTY(Config)
    FString fighter_flight_model_{};

    UPROPERTY(Config)
    FString skater_flight_model_{};

    UPROPERTY(Config)
    FString gunship_flight_model_{};
};

} // namespace ml::ioj
