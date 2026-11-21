#pragma once

#include <ioj/settings/game_anti_aliasing_method.h>

#include "GameFramework/GameUserSettings.h"

#include "SpaceGameUserSettings.generated.h"

namespace ioj {

UCLASS(Config = GameUserSettings)
class SPACEGAME_API USpaceGameUserSettings : public UGameUserSettings {
    GENERATED_BODY()
  public:
    virtual void ApplyNonResolutionSettings() override;
    void SaveSettings() override {}

    auto anti_aliasing_method() const -> GameAntiAliasingMethod;
    void set_anti_aliasing_method(GameAntiAliasingMethod value);

    auto bloom_enabled() const -> bool;
    auto motion_blur_enabled() const -> bool;
    void set_bloom_enabled(bool value);
    void set_motion_blur_enabled(bool value);

  private:
    GameAntiAliasingMethod anti_aliasing_method_{GameAntiAliasingMethod::Off};

    bool bloom_enabled_{true};

    bool motion_blur_enabled_{};
};

} // namespace ioj
