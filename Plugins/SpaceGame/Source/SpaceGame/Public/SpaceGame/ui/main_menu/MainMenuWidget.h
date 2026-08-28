#pragma once

#include "Blueprint/UserWidget.h"

#include "MainMenuWidget.generated.h"

class UButton;
class UOverlay;
class UVerticalBox;
class UWidgetSwitcher;

namespace ml::ioj {
class ULevelSelectWidget;
class UOptionsWidget;

enum class EMainMenuPage : uint8 {
    Main,
    LevelSelect,
    Options,
};

UCLASS()
class SPACEGAME_API UMainMenuWidget : public UUserWidget {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto get_active_page() const noexcept -> EMainMenuPage { return active_page_; }

    void focus_primary_action();
  protected:
    void NativeOnInitialized() override;
    void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* page_switcher{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* main_page{nullptr};
    UPROPERTY(meta = (BindWidget))
    ULevelSelectWidget* level_select_widget{nullptr};
    UPROPERTY(meta = (BindWidget))
    UOptionsWidget* options_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* play_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* options_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* quit_button{nullptr};
  private:
    UFUNCTION()
    void handle_play();
    UFUNCTION()
    void handle_options();
    UFUNCTION()
    void handle_quit();

    void return_from_level_select();
    void return_from_options();
    void set_active_page(EMainMenuPage page);

    EMainMenuPage active_page_{EMainMenuPage::Main};
};
}
