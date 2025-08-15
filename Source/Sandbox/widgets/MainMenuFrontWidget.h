#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"

#include "MainMenuFrontWidget.generated.h"

class UMainMenuWidget;

UCLASS()
class SANDBOX_API UMainMenuFrontWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    virtual bool Initialize() override;

    UPROPERTY()
    UMainMenuWidget* parent_menu{nullptr};
  protected:
    UPROPERTY(meta = (BindWidget))
    UButton* play_button;

    UPROPERTY(meta = (BindWidget))
    UButton* quit_button;
  private:
    UFUNCTION()
    void on_play_button_clicked();
    UFUNCTION()
    void on_quit_button_clicked();
};
