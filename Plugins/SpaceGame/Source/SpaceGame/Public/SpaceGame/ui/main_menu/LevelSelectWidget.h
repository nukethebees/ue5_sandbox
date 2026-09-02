#pragma once

#include "Blueprint/UserWidget.h"
#include "SandboxGameShared/ui/CommonMenuDelegates.h"

#include "LevelSelectWidget.generated.h"

class UButton;
class UOverlay;

namespace ml::ioj {
UCLASS()
class SPACEGAME_API ULevelSelectWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    virtual void activate();
    void focus_back_button();

    FBackRequested back_requested;
  protected:
    void NativeOnInitialized() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* back_button{nullptr};
  private:
    UFUNCTION()
    void handle_back();
};
}
