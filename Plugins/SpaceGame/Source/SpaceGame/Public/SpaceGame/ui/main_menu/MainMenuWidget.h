#pragma once

#include "SpaceGame/ui/common/MenuActivatableWidget.h"
#include "SpaceGamePresentation/ui/style/GameUiStyle.h"
#include <ioj/input/logical/actions.h>

#include <optional>

#include "MainMenuWidget.generated.h"

namespace ioj {
struct MenuUpdate;
class SMainMenuView;
class UGameUiStyleSubsystem;
class ULevelSelectWidget;
class UOptionsWidget;
class USaveGameViewerWidget;

UCLASS()
class SPACEGAME_API UMainMenuWidget : public UMenuActivatableWidget {
    GENERATED_BODY()
  public:
    UMainMenuWidget();
    void present(MenuUpdate const& update);
    void focus_item(MainMenuItem item);
    void activate_item(MainMenuItem item);

    void prepare_for_open(TSubclassOf<ULevelSelectWidget> level_select_class);
  protected:
    void NativeOnInitialized() override;
    auto RebuildWidget() -> TSharedRef<SWidget> override;
    void ReleaseSlateResources(bool release_children) override;
    auto focus_target() const -> UWidget* override;
    auto NativeOnFocusReceived(FGeometry const& geometry, FFocusEvent const& focus_event)
        -> FReply override;
  private:
    void create_content_widgets();
    void show_page(MainMenuItem page);
    void quit_game();
    void restore_focus();
    void handle_page_modal_changed(bool visible);

    UPROPERTY(Transient)
    UGameUiStyleSubsystem* style_{nullptr};
    UPROPERTY(Transient)
    ULevelSelectWidget* level_select_widget_{nullptr};
    UPROPERTY(Transient)
    USaveGameViewerWidget* save_game_viewer_{nullptr};
    UPROPERTY(Transient)
    UOptionsWidget* options_widget_{nullptr};

    TSubclassOf<ULevelSelectWidget> level_select_class_{};
    TSharedPtr<SMainMenuView> view_{};
    FGameUiStyle fallback_style_{};
};
} // namespace ioj
