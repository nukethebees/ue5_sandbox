#pragma once

#include "GameFramework/GameUserSettings.h"
#include "SpaceGame/settings/GameSettingsTypes.h"

#include "SpaceGameUserSettings.generated.h"

namespace ml::ioj {

UCLASS(Config = GameUserSettings)
class SPACEGAME_API USpaceGameUserSettings : public UGameUserSettings {
    GENERATED_BODY()
  public:
    virtual void ApplyNonResolutionSettings() override;
    virtual void SetToDefaults() override;

    auto anti_aliasing_method() const -> EGameAntiAliasingMethod;
    void set_anti_aliasing_method(EGameAntiAliasingMethod value);

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

    void preview_master_volume() const;
  private:
    UPROPERTY(Config)
    int32 anti_aliasing_method_{};

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
};

} // namespace ml::ioj
