#pragma once

#include <SpaceGame/levels/LevelCompletion.h>
#include <SpaceGame/levels/LevelOutcome.h>

#include <Blueprint/UserWidget.h>

#include "GameUiRootLayout.generated.h"

class UOverlay;
namespace ioj {
class UTestBatchGameUiData;
class ULevelSelectWidget;
class ULevelCompletionWidget;
class UMainMenuWidget;
class UPauseMenuWidget;

UCLASS()
class SPACEGAME_API UGameUiRootLayout : public UUserWidget {
    GENERATED_BODY()
  public:
    auto initialise(UTestBatchGameUiData& ui_data) -> bool;
    auto show_main_menu(bool show_level_select, FName preferred_level_id = NAME_None) -> bool;
    auto show_pause_menu() -> UPauseMenuWidget*;
    auto show_level_completion(FLevelCompletion data) -> ULevelCompletionWidget*;
    void clear_menus();
    auto main_menu() const -> UMainMenuWidget*;

  protected:
    UPROPERTY(meta = (BindWidget))
    UOverlay* screen_stack{nullptr};

    UPROPERTY(meta = (BindWidget))
    UOverlay* modal_stack{nullptr};
  private:
    TWeakObjectPtr<UTestBatchGameUiData> ui_data_;
};
}
