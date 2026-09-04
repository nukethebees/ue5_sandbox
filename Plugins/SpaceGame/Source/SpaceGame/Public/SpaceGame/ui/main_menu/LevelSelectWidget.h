#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include "LevelSelectWidget.generated.h"

class UOverlay;

namespace ml::ioj {
class UMenuButtonWidget;

UCLASS()
class SPACEGAME_API ULevelSelectWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    void prepare_for_open(FName preferred_level_id) noexcept;
  protected:
    void NativeOnInitialized() override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* back_button{nullptr};

    [[nodiscard]] auto get_preferred_level_id() const noexcept -> FName {
        return preferred_level_id_;
    }
  private:
    void handle_back();

    FName preferred_level_id_{NAME_None};
};
}
