#pragma once

#include "Blueprint/UserWidget.h"
#include "SandboxGameShared/ui/CommonMenuDelegates.h"

#include "LevelSelectWidget.generated.h"

class UButton;
class UOverlay;
class UTextBlock;

namespace ml::ioj {
UCLASS()
class SPACEGAME_API ULevelSelectWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    void focus_back_button();

    FBackRequested back_requested;
  protected:
    void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UTextBlock* placeholder_text{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* back_button{nullptr};
  private:
    UFUNCTION()
    void handle_back();
};
}
