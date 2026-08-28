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
class USaveGameViewerWidget;

enum class EMainMenuPage : uint8 {
    Main,
    LevelSelect,
    SaveGames,
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
    UVerticalBox* save_games_page{nullptr};
    UPROPERTY(meta = (BindWidget))
    USaveGameViewerWidget* save_game_viewer{nullptr};
    UPROPERTY(meta = (BindWidget))
    UOptionsWidget* options_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* play_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* save_games_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* options_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UButton* quit_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* save_games_back_button{nullptr};
  private:
    UFUNCTION()
    void handle_play();
    UFUNCTION()
    void handle_save_games();
    UFUNCTION()
    void handle_options();
    UFUNCTION()
    void handle_quit();

    void return_from_level_select();
    UFUNCTION()
    void return_from_save_games();
    void return_from_options();
    void set_active_page(EMainMenuPage page);

    EMainMenuPage active_page_{EMainMenuPage::Main};
};
}
