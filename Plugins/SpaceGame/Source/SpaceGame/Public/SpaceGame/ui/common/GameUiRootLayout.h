#pragma once

#include <CommonActivatableWidget.h>

#include "GameUiRootLayout.generated.h"

class UCommonActivatableWidgetStack;
class UInputAction;
class UTestBatchGameUiData;

namespace ml::ioj {
class ULevelSelectWidget;
class ULevelCompletionWidget;
class UMainMenuWidget;
class UPauseMenuWidget;

UCLASS()
class SPACEGAME_API UGameUiRootLayout : public UCommonActivatableWidget {
    GENERATED_BODY()
  public:
    auto initialise(UTestBatchGameUiData& ui_data) -> bool;
    auto show_main_menu(bool show_level_select, FName preferred_level_id = NAME_None) -> bool;
    auto show_pause_menu(UInputAction& toggle_action) -> UPauseMenuWidget*;
    auto show_level_completion(FString level_display_name) -> ULevelCompletionWidget*;
    void clear_menus();

    [[nodiscard]] auto get_active_screen() const -> UCommonActivatableWidget*;
    [[nodiscard]] auto get_active_modal() const -> UCommonActivatableWidget*;
    [[nodiscard]] auto get_screen_count() const -> int32;
    [[nodiscard]] auto get_modal_count() const -> int32;

    TOptional<FUIInputConfig> GetDesiredInputConfig() const override;
  protected:
    UPROPERTY(meta = (BindWidget))
    UCommonActivatableWidgetStack* screen_stack{nullptr};

    UPROPERTY(meta = (BindWidget))
    UCommonActivatableWidgetStack* modal_stack{nullptr};
  private:
    void show_level_select();

    TWeakObjectPtr<UTestBatchGameUiData> ui_data_;
    FName level_select_focus_id_{NAME_None};
};
}
