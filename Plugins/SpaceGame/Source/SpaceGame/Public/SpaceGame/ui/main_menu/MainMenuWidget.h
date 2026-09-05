#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"

#include "MainMenuWidget.generated.h"

class UOverlay;
class UWidgetSwitcher;

namespace ml::ioj {
class UMainMenuLandingWidget;
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
    UMainMenuLandingWidget* main_page{nullptr};
    UPROPERTY(meta = (BindWidget))
    USaveGameViewerWidget* save_game_viewer{nullptr};
    UPROPERTY(meta = (BindWidget))
    UOptionsWidget* options_widget{nullptr};
  private:
    void handle_select_mission();
    void handle_save_games();
    void handle_options();
    void handle_quit();

    void return_from_save_games();
    void return_from_options();
    void set_active_page(EMainMenuPage page);

    EMainMenuPage active_page_{EMainMenuPage::Main};
};
}
