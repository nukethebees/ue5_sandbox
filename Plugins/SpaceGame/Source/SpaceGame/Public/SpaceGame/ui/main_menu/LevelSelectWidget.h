#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include "LevelSelectWidget.generated.h"

namespace ml::ioj {
UCLASS()
class SPACEGAME_API ULevelSelectWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    ULevelSelectWidget();

    void prepare_for_open(FName preferred_level_id) noexcept;
  protected:
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;
    auto NativeOnHandleBackAction() -> bool override;

    [[nodiscard]] auto get_preferred_level_id() const noexcept -> FName {
        return preferred_level_id_;
    }
  private:
    FName preferred_level_id_{NAME_None};
};
}
