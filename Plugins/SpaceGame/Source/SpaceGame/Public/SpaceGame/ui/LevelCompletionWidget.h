#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include "LevelCompletionWidget.generated.h"

class UOverlay;
class UTextBlock;
class UVerticalBox;

namespace ml::ioj {
class UMenuButtonWidget;

DECLARE_MULTICAST_DELEGATE(FReturnToLevelSelectRequested);

UCLASS()
class SPACEGAME_API ULevelCompletionWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    void prepare_for_open(FString level_display_name);

    FReturnToLevelSelectRequested return_to_level_select_requested;
  protected:
    void NativeOnInitialized() override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;
    auto NativeOnHandleBackAction() -> bool override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* level_name_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UVerticalBox* statistics_container{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* return_to_level_select_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* keep_playing_button{nullptr};
  private:
    void handle_return_to_level_select();
    void handle_keep_playing();

    bool action_requested_{false};
};
}
