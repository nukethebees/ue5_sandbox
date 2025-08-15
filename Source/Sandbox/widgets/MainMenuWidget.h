#pragma once

#include <utility>

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Sandbox/widgets/MainMenuFrontWidget.h"
#include "Sandbox/widgets/LevelSelectWidget.h"

#include "MainMenuWidget.generated.h"

UCLASS()
class SANDBOX_API UMainMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    virtual bool Initialize() override;

    void hide_all_widgets();
    void show_front_menu();
    void show_level_select_menu();
  private:
    template <typename Self>
    void show_menu(this Self&& self, auto* menu_ptr) {
        std::forward<Self>(self).hide_all_widgets();
        menu_ptr->SetVisibility(ESlateVisibility::Visible);
    }
  protected:
    UPROPERTY(meta = (BindWidget))
    UMainMenuFrontWidget* front_menu_widget;
    UPROPERTY(meta = (BindWidget))
    ULevelSelectWidget* level_select_widget;
};
