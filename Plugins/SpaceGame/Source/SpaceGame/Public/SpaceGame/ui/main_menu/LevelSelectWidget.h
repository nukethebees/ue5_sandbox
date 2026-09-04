#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include "LevelSelectWidget.generated.h"

class UOverlay;

namespace ml::ioj {
class UMenuButtonWidget;

UCLASS()
class SPACEGAME_API ULevelSelectWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  protected:
    void NativeOnInitialized() override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* back_button{nullptr};
  private:
    void handle_back();
};
}
