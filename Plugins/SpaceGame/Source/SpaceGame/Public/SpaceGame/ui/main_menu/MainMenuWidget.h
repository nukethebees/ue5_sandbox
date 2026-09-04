#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include "MainMenuWidget.generated.h"

class UButton;
class UOverlay;
class UVerticalBox;
class UWidgetSwitcher;

namespace ml::ioj {
class UMenuButtonWidget;
class UOptionsWidget;
class USaveGameViewerWidget;

enum class EMainMenuPage : uint8 {
    Main,
    SaveGames,
    Options,
};

DECLARE_MULTICAST_DELEGATE(FLevelSelectRequested);

UCLASS()
class SPACEGAME_API UMainMenuWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    [[nodiscard]] auto get_active_page() const noexcept -> EMainMenuPage { return active_page_; }

    FLevelSelectRequested level_select_requested;
  protected:
    void NativeOnInitialized() override;
    auto NativeGetDesiredFocusTarget() const -> UWidget* override;
    auto NativeOnHandleBackAction() -> bool override;

    UPROPERTY(meta = (BindWidget, GeneratorRoot))
    UOverlay* root_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UWidgetSwitcher* page_switcher{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* main_page{nullptr};
    UPROPERTY(meta = (BindWidget))
    UVerticalBox* save_games_page{nullptr};
    UPROPERTY(meta = (BindWidget))
    USaveGameViewerWidget* save_game_viewer{nullptr};
    UPROPERTY(meta = (BindWidget))
    UOptionsWidget* options_widget{nullptr};

    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* play_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* save_games_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* options_button{nullptr};
    UPROPERTY(meta = (BindWidget))
    UMenuButtonWidget* quit_button{nullptr};

    UPROPERTY(meta = (BindWidget))
    UButton* save_games_back_button{nullptr};
  private:
    void handle_play();
    void handle_save_games();
    void handle_options();
    void handle_quit();

    UFUNCTION()
    void return_from_save_games();
    void return_from_options();
    void set_active_page(EMainMenuPage page);

    EMainMenuPage active_page_{EMainMenuPage::Main};
    UPROPERTY(Transient)
    TObjectPtr<UWidget> main_focus_target_{nullptr};
};
}
